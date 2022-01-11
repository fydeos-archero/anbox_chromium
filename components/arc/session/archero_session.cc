#include "components/arc/session/archero_session.h"

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
// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
#include "media/audio/simple_sources.h"
#endif
// [AUD] Remove old audio arch --


#define USE_PROTOBUF_CALLBACK_HEADER

namespace arc{

// [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
class AudioServer:
  public archero::ArcHeroAudio,
  public media::AudioOutputStream::AudioSourceCallback,
  public std::enable_shared_from_this<AudioServer>{
public:
  AudioServer(){}

  void Initialize(){
    content::BrowserMainLoop::GetAudioManager()->GetTaskRunner()->PostTask(FROM_HERE,
      base::BindOnce(&AudioServer::Initialize2, shared_from_this())
    );

    accept();
  }

  void Initialize2(){
    audio_params_.reset(
      new media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LINEAR,
        media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate,
        frames_per_buffer_
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

  void process_data(uint8_t *buffer, uint32_t size, uint32_t *remain_size) override {
    {
      base::AutoLock l(lock_);

      ARCHERO_LOGI("=== AudioServer::process_data %p %d", buffer, size);

      buffer_queue_.insert(buffer_queue_.end(), buffer, buffer + size);

      if (remain_size){
        *remain_size = 0;
      }
    }

    while(buffer_queue_.size() > frames_per_buffer_ * audio_params_->channels() * sizeof(int16_t) * 2){
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // archero::graphics::Buffer buffer{data.data(), data.data() + data.size()};
    // LOG(INFO) << "=== ArcHeroSession::write_data audio frames " << (data.size() / audio_params_.get()->channels() / sizeof(int16_t));
    // queue_.push_locked(std::move(buffer), l);
  }

  int OnMoreData(base::TimeDelta /*delay*/,
                           base::TimeTicks /*delay_timestamp*/,
                           int /*prior_frames_skipped*/,
                           media::AudioBus* dest) override {

    base::AutoLock l(lock_);
    int wanted = dest->frames() * dest->channels() * sizeof(int16_t);
    // std::unique_ptr<uint8_t[]> dst;
    // int count = 0;

    // while (count < wanted) {
    //   if (dst.get() == nullptr){
    //     dst = std::make_unique<uint8_t[]>(wanted);
    //   }
    //   if (read_buffer_left_ > 0) {
    //     size_t avail = std::min<size_t>(wanted - count, read_buffer_left_);
    //     memcpy(dst.get() + count,
    //           read_buffer_.data() + (read_buffer_.size() - read_buffer_left_),
    //           avail);
    //     count += avail;
    //     read_buffer_left_ -= avail;
    //     continue;
    //   }

    //   bool blocking = (count == 0);
    //   auto result = -EIO;
    //   if (blocking)
    //     result = queue_.pop_locked(&read_buffer_, l);
    //   else
    //     result = queue_.try_pop_locked(&read_buffer_);

    //   if (result == 0) {
    //     read_buffer_left_ = read_buffer_.size();
    //     continue;
    //   }

    //   break;
    // }

    auto count = std::min<size_t>(buffer_queue_.size(), wanted);
    int use_frames = count / dest->channels() / sizeof(int16_t);

    if (count > 0){
      ARCHERO_LOGI("=== AudioServer::OnMoreData count: %d, buffer_queue_: %d, use_frames: %d", (int)count, (int)buffer_queue_.size(), use_frames);

      dest->FromInterleaved<media::SignedInt16SampleTypeTraits>((int16_t*)buffer_queue_.data(), use_frames);

      buffer_queue_.erase(buffer_queue_.begin(), buffer_queue_.begin() +  count);
    }

    return use_frames;
  }

  void OnError(ErrorType type) override {LOG(INFO) << "==== ArcHeroSession::OnError " << (int)type;}

private:
  const int frames_per_buffer_ = 1024;

  std::unique_ptr<media::AudioOutputStream> audio_out_stream_;
  std::unique_ptr<media::AudioParameters> audio_params_;

  base::Lock lock_;
  std::vector<std::uint8_t> buffer_queue_;
};
#endif

// class PlatformMessageProcessor2:
//   public archero::rpc::MessageProcessor {
//  public:
//   PlatformMessageProcessor2(const std::shared_ptr<archero::network::MessageSender> &sender,
//     const std::shared_ptr<archero::rpc::PendingCallCache> &pending_calls,
//     const std::shared_ptr<ArcHeroSession> &archero_session
//     ):
//     archero::rpc::MessageProcessor(sender, pending_calls),
//     archero_session_(archero_session){
//   }

//   ~PlatformMessageProcessor2(){
//   }

//   void dispatch(archero::rpc::Invocation const &invocation) override {
//     LOG(INFO) << "===== PlatformMessageProcessor2::dispatch " << invocation.method_name();

//     if (invocation.method_name() == "app_created"){
//       invoke(this, this, &PlatformMessageProcessor2::app_created, invocation);
//     }else if (invocation.method_name() == "app_removed"){
//       invoke(this, this, &PlatformMessageProcessor2::app_removed, invocation);
//     }else if (invocation.method_name() == "task_created"){
//       invoke(this, this, &PlatformMessageProcessor2::task_created, invocation);
//     }else if (invocation.method_name() == "task_removed"){
//       invoke(this, this, &PlatformMessageProcessor2::task_removed, invocation);
//     }
//   }

//   void app_created(archero::protobuf::bridge::Application const *request,
//                                      archero::protobuf::rpc::Void *response,
//                                      google::protobuf::Closure *done) {
//     LOG(INFO) << "===== PlatformMessageProcessor2::app_created " << request->name() << " " << request->package() << " " << request->icon().size();


//     for (auto& observer: archero_session_->GetObserver()){
//       observer.onAppAdded(const_cast<archero::protobuf::bridge::Application*>(request));
//     }
//   }

//   void app_removed(archero::protobuf::bridge::Application const *request,
//                                      archero::protobuf::rpc::Void *response,
//                                      google::protobuf::Closure *done) {
//     LOG(INFO) << "===== PlatformMessageProcessor2::app_removed " << request->package() << " " << request->launch_intent().component();

//     for (auto& observer: archero_session_->GetObserver()){
//       observer.onAppRemoved(const_cast<archero::protobuf::bridge::Application*>(request));
//     }
//   }

//   void task_created(archero::protobuf::chrome::CreatedTask const *request,
//                                      archero::protobuf::rpc::Void *response,
//                                      google::protobuf::Closure *done) {

//     LOG(INFO) << "===== PlatformMessageProcessor2::task_created " << request->title() << " " << request->package_name();

//     // base::CreateSingleThreadTaskRunner p1({content::BrowserThread::UI});
//     // base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
//     // base::ThreadPool::PostTask(FROM_HERE,
//     // base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
//     // base::PostTask(FROM_HERE, {content::BrowserThread::UI},
//     base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})->PostTask(FROM_HERE,
//       base::BindOnce(&PlatformMessageProcessor2::task_created2, weak_ptr_factory_.GetWeakPtr(), request->id(), std::string(request->title()), std::string(request->package_name()))
//     );
//   }

//   void task_created2(int task_id, const std::string &name, const std::string &package){
//     for (auto& observer: archero_session_->GetObserver()){
//       // observer.OnTaskCreated(request->id(), request->title(), request->package_name());
//       observer.OnTaskCreated(task_id, name, package);
//     }
//   }

//   void task_removed(archero::protobuf::chrome::RemovedTask const *request,
//                                      archero::protobuf::rpc::Void *response,
//                                      google::protobuf::Closure *done) {
//     for (auto& observer: archero_session_->GetObserver()){
//       observer.OnTaskRemoved(request->id());
//     }
//   }

//   void process_event_sequence(const std::string &raw_events) override {
//     LOG(INFO) << "===== PlatformMessageProcessor2::process_event_sequence";
//   }

// private:
//   std::shared_ptr<ArcHeroSession> archero_session_;

//   base::WeakPtrFactory<PlatformMessageProcessor2> weak_ptr_factory_{this};
// };

void ArcHeroSession::Initialize(Profile* profile){
  profile_ = profile;

  chmod("/run/chrome/wayland-0", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  chmod("/run/chrome/wayland-0.lock", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

  accept(archero::ArcHeroBridge::chromium_socket_path());
  // set_booted_callback([]{
  //   LOG(INFO) << "====== ArcHeroSession::Initialize booted ";
  // });

  // archero::Log().Init(archero::Logger::Severity::kDebug);

  // mkdir("/run/chrome/archero/",        S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO);
  // mkdir("/run/chrome/archero/sockets", S_IRWXU|S_IRGRP|S_IWGRP|S_IRWXO);

  // [AUD] Remove old audio arch ++
#ifdef OLD_AUDIO_ARCH
  audio_server_ = std::make_shared<AudioServer>();
  audio_server_->Initialize();
#endif
  // [AUD] Remove old audio arch --

  // audio_server_ = std::make_shared<AudioServer>(rt_);
  // audio_server_->Initialize();

  // publish_socket_ = std::make_shared<archero::network::PublishedSocketConnector>(
  //   std::string(ARCHERO_RPC_CHROME_NAME), rt_,
  //   std::make_shared<archero::rpc::ConnectionCreator>(rt_, [&](const std::shared_ptr<archero::network::MessageSender> &sender) {

  //     LOG(INFO) << "====== ArcHeroSession::Initialize connected";

  //     auto pendding_call = std::make_shared<archero::rpc::PendingCallCache>();

  //     channel_ = std::make_shared<archero::rpc::Channel>(pendding_call, sender);

  //     return std::make_shared<PlatformMessageProcessor2>(
  //       sender,
  //       pendding_call,
  //       shared_from_this()
  //     );
  //   })
  // );

  // chmod("/run/chrome/archero/sockets/archero_chrome", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  // (void)chown("/run/chrome/archero/sockets/archero_chrome", 656360, 656360);

  // rt_->start();

  chromeos::SessionManagerClient::Get()->StartArcHeroContainer(
    base::BindOnce(&ArcHeroSession::OnArcHeroInstanceStarted, weak_factory_.GetWeakPtr())
  );

  chromeos::SessionManagerClient::Get()->AddObserver(this);
}

bool ArcHeroSession::LaunchApp(std::string &name, std::string &package, std::string &component){
  LOG(INFO) << "====== ArcHeroSession::LaunchApp " << package << " " << component;

  // {
  //   std::lock_guard<decltype(mutex_)> lock(mutex_);
  //   launch_wait_handle_.expect_result();
  // }

  send_launch_app(package.data(), component.data());

  // auto c = std::make_shared<archero::protobuf::rpc::Void>();
  // archero::protobuf::bridge::LaunchApplication message;
  // message.set_stack(::archero::protobuf::bridge::LaunchApplication_Stack_FREEFORM);

  // auto launch_intent = message.mutable_intent();
  // launch_intent->set_component(component);
  // launch_intent->set_package(package);

  // channel_->call_method(
  //   "launch_application", &message, c.get(),
  //   google::protobuf::NewCallback(this, &ArcHeroSession::callback)
  // );

  // launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  // if (!launch_wait_handle_.has_result()){
  //   LOG(INFO) <<  "====== ArcHeroSession::LaunchApp failed " << package << " " << component;
  // }

  return true;
}

bool ArcHeroSession::InstallApp(const std::string &file_path){

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
    base::BindOnce(&ArcHeroSession::InstallApp2, weak_factory_.GetWeakPtr(), file_path)
  );

  return true;
}

void ArcHeroSession::InstallApp2(const std::string &file_path){
  auto filePath = base::FilePath(file_path);
  base::File infile(filePath, base::File::FLAG_OPEN | base::File::FLAG_READ);

  LOG(INFO) << "==== ArcHeroSession::InstallApp " << filePath.BaseName();

  int file_pos = 0;
  char bytes[1024 * 8];
  for (;;){
    auto readed_size = infile.Read(file_pos, bytes, sizeof(bytes));
    if (readed_size <= 0){
      send_install_app(filePath.BaseName().value().data(), nullptr, file_pos, 0);
      break;
    }

    send_install_app(filePath.BaseName().value().data(), bytes, file_pos, readed_size);
    file_pos += readed_size;
  }
}

void ArcHeroSession::Uninstall(const std::string &package, const std::string &component){
  LOG(INFO) << "====== ArcHeroSession::Uninstall " << package << " " << component;

  // {
  //   std::lock_guard<decltype(mutex_)> lock(mutex_);
  //   launch_wait_handle_.expect_result();
  // }

  // auto c = std::make_shared<archero::protobuf::rpc::Void>();
  // archero::protobuf::chrome::UninstallApp message;
  // message.set_package_name(package);
  // message.set_component_name(component);

  // channel_->call_method(
  //   "uninstall_app", &message, c.get(),
  //   google::protobuf::NewCallback(this, &ArcHeroSession::callback)
  // );

  // launch_wait_handle_.wait_for_pending(std::chrono::milliseconds{30 * 1000});
  // if (!launch_wait_handle_.has_result()){
  //   LOG(INFO) <<  "====== ArcHeroSession::Uninstall failed ";
  // }
}

void ArcHeroSession::Close(int task_id){
  LOG(INFO) << "====== ArcHeroSession::Close " << task_id;

  //
}

void ArcHeroSession::ArcInstanceStopped(login_manager::ArcContainerStopReason reason){
  LOG(INFO) << "===== ArcHeroSession::ArcInstanceStopped: " << (int)reason;

  chromeos::SessionManagerClient::Get()->StartArcHeroContainer(
    base::BindOnce(&ArcHeroSession::OnArcHeroInstanceStarted, weak_factory_.GetWeakPtr())
  );
}

}
