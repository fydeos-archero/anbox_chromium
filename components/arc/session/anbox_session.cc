#include "components/arc/session/anbox_session.h"

#include <queue>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "net/base/io_buffer.h"
#include "media/audio/simple_sources.h"

#define USE_PROTOBUF_CALLBACK_HEADER

#include "anbox/application/database.h"
#include "anbox/bridge/android_api_stub.h"
#include "anbox/bridge/platform_api_skeleton.h"
#include "anbox/network/delegate_connection_creator.h"
#include "anbox/network/published_socket_connector.h"
#include "anbox/network/local_socket_messenger.h"
#include "anbox/bridge/platform_message_processor.h"
#include "anbox/graphics/buffer_queue.h"
#include "anbox/rpc/channel.h"
#include "anbox/rpc/connection_creator.h"
#include "anbox/rpc/template_message_processor.h"
#include "anbox/runtime.h"
#include "anbox/logger.h"

#include "anbox/graphics/rect.h"

#include "anbox_chrome.pb.h"
#include "anbox_rpc.pb.h"
#include "anbox_bridge.pb.h"

#include "fydeos/anbox_rpc_chrome.h"
#include "anbox/audio/client_info.h"

#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace arc{

class AudioForwarder : public anbox::network::MessageProcessor {
 public:
  AudioForwarder(const std::shared_ptr<anbox::audio::Sink> &sink) :
    sink_(sink) {
  }

  bool process_data(const std::vector<std::uint8_t> &data) override {
    sink_->write_data(data);
    return true;
  }

 private:
  std::shared_ptr<anbox::audio::Sink> sink_;
};

class AudioServer:
  public anbox::audio::Sink,
  public media::AudioOutputStream::AudioSourceCallback,
  public std::enable_shared_from_this<AudioServer> {
public:
  AudioServer(const std::shared_ptr<anbox::Runtime>& rt):     
    socket_file_("/run/chrome/anbox/sockets/anbox_audio"),
    connector_(std::make_shared<anbox::network::PublishedSocketConnector>(
              socket_file_, rt,
              std::make_shared<anbox::network::DelegateConnectionCreator<boost::asio::local::stream_protocol>>(std::bind(&AudioServer::create_connection_for, this, std::placeholders::_1)))),
    connections_(std::make_shared<anbox::network::Connections<anbox::network::SocketConnection>>()),
    next_id_(0),
    queue_(16){

    // FIXME: currently creating the socket creates it with the rights of
    // the user we're running as. As this one is mapped into the container
    ::chmod(socket_file_.c_str(), 0777);    
  }
  ~AudioServer(){}

  // std::string socket_file() const { return socket_file_; }

public:  
  void Initialize(){
    content::BrowserMainLoop::GetAudioManager()->GetTaskRunner()->PostTask(FROM_HERE,  
      base::BindOnce(&AudioServer::Initialize2, shared_from_this())
    );
  }

  void Initialize2(){
    audio_params_.reset(
      new media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LINEAR, 
        media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate,        
        1024
      )
    );

    audio_out_stream_.reset(
      content::BrowserMainLoop::GetAudioManager()->MakeAudioOutputStream(
        *audio_params_.get(), 
        std::string(), 
        media::AudioManager::LogCallback()
      )  //media::AudioDeviceDescription::kDefaultDeviceId, base::DoNothing());        
    );    
    
    audio_out_stream_->Open();
    // audio_out_stream_->SetVolume(0.5);

    audio_out_stream_->Start(this);
    // audio_out_stream_->Start(new media::BeepingSource(media::AudioParameters(
    //   media::AudioParameters::AUDIO_PCM_LINEAR, 
    //   media::ChannelLayout::CHANNEL_LAYOUT_MONO,
    //   media::AudioParameters::kAudioCDSampleRate,
    //   media::AudioParameters::kAudioCDSampleRate / 10
    // )));
  }

  void write_data(const std::vector<std::uint8_t> &data) override{    
    std::unique_lock<std::mutex> l(lock_);    
          
    anbox::graphics::Buffer buffer{data.data(), data.data() + data.size()};            
    LOG(INFO) << "=== AnboxSession::write_data audio frames " << (data.size() / audio_params_.get()->channels() / sizeof(int16_t));
    queue_.push_locked(std::move(buffer), l);    
  }  

  int OnMoreData(base::TimeDelta /*delay*/,
                           base::TimeTicks /*delay_timestamp*/,
                           int /*prior_frames_skipped*/,
                           media::AudioBus* dest) override {

    std::unique_lock<std::mutex> l(lock_);
    //int wanted = media::AudioBus::CalculateMemorySize(2, dest->frames());
    int wanted = dest->frames() * dest->channels() * sizeof(int16_t);
    std::unique_ptr<uint8_t[]> dst;    
    int count = 0;

    while (count < wanted) {
      if (dst.get() == nullptr){        
        dst = std::make_unique<uint8_t[]>(wanted);
      }
      if (read_buffer_left_ > 0) {
        size_t avail = std::min<size_t>(wanted - count, read_buffer_left_);        
        memcpy(dst.get() + count,
              read_buffer_.data() + (read_buffer_.size() - read_buffer_left_),
              avail);
        count += avail;
        read_buffer_left_ -= avail;
        continue;
      }

      bool blocking = (count == 0);
      auto result = -EIO;
      if (blocking)
        result = queue_.pop_locked(&read_buffer_, l);
      else
        result = queue_.try_pop_locked(&read_buffer_);

      if (result == 0) {
        read_buffer_left_ = read_buffer_.size();
        continue;
      }

      break;
    }

    int use_frames = count / dest->channels() / sizeof(int16_t);
    
    if (count > 0){
      LOG(INFO) << "==== AnboxSession::OnMoreData frames " << use_frames;
      dest->FromInterleaved<media::SignedInt16SampleTypeTraits>((int16_t*)dst.get(), use_frames);

      // media::AudioBus::WrapMemory(2, use_frames, dst.get())->CopyTo(dest);      
    }    
    
    return use_frames;
  }  

  void OnError(ErrorType type) override {LOG(INFO) << "==== AnboxSession::OnError " << (int)type;}                        
private:
  void create_connection_for(std::shared_ptr<boost::asio::basic_stream_socket<
                             boost::asio::local::stream_protocol>> const& socket){
    auto const messenger =
        std::make_shared<anbox::network::LocalSocketMessenger>(socket);

    // We have to read the client flags first before we can continue
    // processing the actual commands
    anbox::audio::ClientInfo client_info;
    auto err = messenger->receive_msg(
        boost::asio::buffer(&client_info, sizeof(anbox::audio::ClientInfo)));
    if (err) {
      ERROR("Failed to read client info: %s", err.message());
      return;
    }

    std::shared_ptr<anbox::network::MessageProcessor> processor;

    switch (client_info.type) {
    case anbox::audio::ClientInfo::Type::Playback:
      // processor = std::make_shared<AudioForwarder>(platform_->create_audio_sink());
      processor = std::make_shared<AudioForwarder>(shared_from_this());
      break;
    case anbox::audio::ClientInfo::Type::Recording:
      break;
    default:
      ERROR("Invalid client type %d", static_cast<int>(client_info.type));
      return;
    }

    // Everything ok, so approve the client by sending the requesting client
    // info back. Once we have more things to negotiate we will send a modified
    // client info struct back.
    messenger->send(reinterpret_cast<char*>(&client_info), sizeof(client_info));

    auto connection = std::make_shared<anbox::network::SocketConnection>(
          messenger, messenger, next_id(), connections_, processor);
    connections_->add(connection);

    connection->read_next_message();                           
  }

  int next_id(){ return next_id_.fetch_add(1);}
  
private:
  std::string socket_file_;
  std::shared_ptr<anbox::network::PublishedSocketConnector> connector_;
  std::shared_ptr<anbox::network::Connections<anbox::network::SocketConnection>> const connections_;
  std::atomic<int> next_id_;  

  std::unique_ptr<media::AudioOutputStream> audio_out_stream_;  
  std::unique_ptr<media::AudioParameters> audio_params_;  

  std::mutex lock_;
  anbox::graphics::BufferQueue queue_;  
  anbox::graphics::Buffer read_buffer_;
  size_t read_buffer_left_ = 0;
};  

class PlatformMessageProcessor2: 
  public anbox::rpc::MessageProcessor {
 public:
  PlatformMessageProcessor2(const std::shared_ptr<anbox::network::MessageSender> &sender,    
    const std::shared_ptr<anbox::rpc::PendingCallCache> &pending_calls,
    const std::shared_ptr<AnboxSession> &anbox_session
    ):
    anbox::rpc::MessageProcessor(sender, pending_calls),
    anbox_session_(anbox_session){        
  }      

  ~PlatformMessageProcessor2(){    
  }

  void dispatch(anbox::rpc::Invocation const &invocation) override {
    LOG(INFO) << "===== PlatformMessageProcessor2::dispatch " << invocation.method_name();

    if (invocation.method_name() == "app_created"){
      invoke(this, this, &PlatformMessageProcessor2::app_created, invocation);
    }else if (invocation.method_name() == "app_removed"){
      invoke(this, this, &PlatformMessageProcessor2::app_removed, invocation);
    }else if (invocation.method_name() == "task_created"){    
      invoke(this, this, &PlatformMessageProcessor2::task_created, invocation);
    }else if (invocation.method_name() == "task_removed"){    
      invoke(this, this, &PlatformMessageProcessor2::task_removed, invocation);
    }
  }

  void app_created(anbox::protobuf::bridge::Application const *request,
                                     anbox::protobuf::rpc::Void *response,
                                     google::protobuf::Closure *done) {                                       
    LOG(INFO) << "===== PlatformMessageProcessor2::app_created " << request->name() << " " << request->package() << " " << request->icon().size();

    
    for (auto& observer: anbox_session_->GetObserver()){
      observer.onAppAdded(const_cast<anbox::protobuf::bridge::Application*>(request));
    }
  }

  void app_removed(anbox::protobuf::bridge::Application const *request,
                                     anbox::protobuf::rpc::Void *response,
                                     google::protobuf::Closure *done) {
    LOG(INFO) << "===== PlatformMessageProcessor2::app_removed " << request->package() << " " << request->launch_intent().component();

    for (auto& observer: anbox_session_->GetObserver()){
      observer.onAppRemoved(const_cast<anbox::protobuf::bridge::Application*>(request));
    }
  }

  void task_created(anbox::protobuf::chrome::CreatedTask const *request,
                                     anbox::protobuf::rpc::Void *response,
                                     google::protobuf::Closure *done) {    

    LOG(INFO) << "===== PlatformMessageProcessor2::task_created " << request->title() << " " << request->package_name();

    // base::CreateSingleThreadTaskRunner p1({content::BrowserThread::UI});
    // base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,  
    // base::ThreadPool::PostTask(FROM_HERE,
    // base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
    // base::PostTask(FROM_HERE, {content::BrowserThread::UI},    
    base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})->PostTask(FROM_HERE,
      base::BindOnce(&PlatformMessageProcessor2::task_created2, weak_ptr_factory_.GetWeakPtr(), request->id(), std::string(request->title()), std::string(request->package_name()))
    );      
  }  

  void task_created2(int task_id, const std::string &name, const std::string &package){
    for (auto& observer: anbox_session_->GetObserver()){
      // observer.OnTaskCreated(request->id(), request->title(), request->package_name());
      observer.OnTaskCreated(task_id, name, package);
    }
  }

  void task_removed(anbox::protobuf::chrome::RemovedTask const *request,
                                     anbox::protobuf::rpc::Void *response,
                                     google::protobuf::Closure *done) {    
    for (auto& observer: anbox_session_->GetObserver()){
      observer.OnTaskRemoved(request->id());
    }
  }

  void process_event_sequence(const std::string &raw_events) override {
    LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence";          
  }  

private:  
  std::shared_ptr<AnboxSession> anbox_session_;   

  base::WeakPtrFactory<PlatformMessageProcessor2> weak_ptr_factory_{this}; 
};

AnboxSession::AnboxSession(): 
  rt_(anbox::Runtime::create()){           
}

AnboxSession::~AnboxSession(){}           

void AnboxSession::Initialize(Profile* profile){  
  profile_ = profile;  

  anbox::Log().Init(anbox::Logger::Severity::kDebug);      

  mkdir("/run/chrome/anbox/",        S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO);
  mkdir("/run/chrome/anbox/sockets", S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO);   
  
  audio_server_ = std::make_shared<AudioServer>(rt_);  
  audio_server_->Initialize();  

  publish_socket_ = std::make_shared<anbox::network::PublishedSocketConnector>(    
    std::string(ANBOX_RPC_CHROME_NAME), rt_,
    std::make_shared<anbox::rpc::ConnectionCreator>(rt_, [&](const std::shared_ptr<anbox::network::MessageSender> &sender) {

      LOG(INFO) << "====== AnboxSession::Initialize connected";          

      auto pendding_call = std::make_shared<anbox::rpc::PendingCallCache>();

      channel_ = std::make_shared<anbox::rpc::Channel>(pendding_call, sender);                    
            
      return std::make_shared<PlatformMessageProcessor2>(
        sender,
        pendding_call,
        shared_from_this()
      );
    })
  );

  chmod("/run/chrome/anbox/sockets/anbox_chrome", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  (void)chown("/run/chrome/anbox/sockets/anbox_chrome", 656360, 656360);  

  rt_->start();  

  chromeos::SessionManagerClient::Get()->StartAnboxContainer(
    base::BindOnce(&AnboxSession::OnAnboxInstanceStarted,
                                       weak_factory_.GetWeakPtr())
  );
}

void AnboxSession::OnAnboxInstanceStarted(bool result) {
  LOG(INFO) << "====== AnboxSession::OnAnboxInstanceStarted " << result;
}

bool AnboxSession::LaunchApp(std::string &name, std::string &package, std::string &component){
  LOG(INFO) << "====== AnboxSession::LaunchApp " << package << " " << component;

  {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    launch_wait_handle_.expect_result();
  }

  auto c = std::make_shared<anbox::protobuf::rpc::Void>();    
  anbox::protobuf::bridge::LaunchApplication message;  
  message.set_stack(::anbox::protobuf::bridge::LaunchApplication_Stack_FREEFORM);

  auto launch_intent = message.mutable_intent();
  launch_intent->set_component(component);
  launch_intent->set_package(package);                

  channel_->call_method(
    "launch_application", &message, c.get(),
    google::protobuf::NewCallback(this, &AnboxSession::callback)
  );    

  launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  if (!launch_wait_handle_.has_result()){    
    LOG(INFO) <<  "====== AnboxSession::LaunchApp failed " << package << " " << component;
  }

  return true;
}

bool AnboxSession::InstallApp(const std::string &file_path){

  // GetFileSystemURL(name);  
  // storage::FileSystemContext* file_system_context =
  //   file_manager::util::GetFileSystemContextForExtensionId(profile_, file_manager::kFileManagerAppId);  

  //   std::unique_ptr<storage::FileStreamReader> reader = 
  //     file_system_context->CreateFileStreamReader(file_path, 0,  std::numeric_limits<int64_t>::max(), base::Time());
  
  // AsyncFileTestHelper::GetMetadata(
                                      //  file_system_context_.get(), url, &info)
  // for (;;){
  //   scoped_refptr<net::IOBufferWithSize> buf(
  //       base::MakeRefCounted<net::IOBufferWithSize>(size - total_bytes_read));

  //   int rv = reader->Read(buf.get(), buf->size(), callback.callback());    
  // }

  // base::ThreadPool::PostTask(FROM_HERE, {content::BrowserThread::IO},
  base::CreateSingleThreadTaskRunner({content::BrowserThread::IO})->PostTask(FROM_HERE,
    base::BindOnce(&AnboxSession::InstallApp2, weak_factory_.GetWeakPtr(), file_path)
  );  

  return true;
}

void AnboxSession::InstallApp2(const std::string &file_path){
  auto filePath = base::FilePath(file_path);    
  base::File infile(filePath, base::File::FLAG_OPEN | base::File::FLAG_READ);

  LOG(INFO) << "==== AnboxSession::InstallApp " << filePath.BaseName();  

  int file_pos = 0;
  char bytes[1024 * 8];
  for (;;){
    {
      std::lock_guard<decltype(mutex_)> lock(mutex_);
      launch_wait_handle_.expect_result();
    }

    anbox::protobuf::chrome::InstallApp message;  

    message.set_name(filePath.BaseName().value());
    message.set_pos(file_pos);
    auto data = message.mutable_data();

    auto c = std::make_shared<anbox::protobuf::rpc::Void>();    

    auto readed_size = infile.Read(file_pos, bytes, sizeof(bytes));
    if (readed_size <= 0){
      message.set_size(readed_size);

      channel_->call_method("install_app", &message, 
        c.get(),
        google::protobuf::NewCallback(this, &AnboxSession::callback)
      );

      launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
      break;
    }    

    message.set_size(readed_size);

    data->append(bytes, readed_size);

    file_pos += readed_size;

    channel_->call_method("install_app", &message, 
      c.get(),
      google::protobuf::NewCallback(this, &AnboxSession::callback)
    );

    launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  }    
}

void AnboxSession::Uninstall(const std::string &package, const std::string &component){
  LOG(INFO) << "====== AnboxSession::Uninstall " << package << " " << component;

  {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    launch_wait_handle_.expect_result();
  }

  auto c = std::make_shared<anbox::protobuf::rpc::Void>();    
  anbox::protobuf::chrome::UninstallApp message;  
  message.set_package_name(package);
  message.set_component_name(component);

  channel_->call_method(
    "uninstall_app", &message, c.get(),
    google::protobuf::NewCallback(this, &AnboxSession::callback)
  );

  launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  if (!launch_wait_handle_.has_result()){    
    LOG(INFO) <<  "====== AnboxSession::Uninstall failed ";
  }
}

void AnboxSession::Close(int task_id){
  LOG(INFO) << "====== AnboxSession::Close " << task_id;

  {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    launch_wait_handle_.expect_result();
  }

  auto c = std::make_shared<anbox::protobuf::rpc::Void>();    
  anbox::protobuf::bridge::RemoveTask message;  
  message.set_id(task_id);  

  channel_->call_method(
    "remove_task", &message, c.get(),
    google::protobuf::NewCallback(this, &AnboxSession::callback)
  );    

  launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  if (!launch_wait_handle_.has_result()){    
    LOG(INFO) <<  "====== AnboxSession::Close failed ";
  }
}

}