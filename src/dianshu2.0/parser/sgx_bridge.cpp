#include "sgx_bridge.h"
#include "iodef.h"
#include <boost/filesystem.hpp>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>
#include <random>

using stx_status = stbox::stx_status;
std::shared_ptr<dianshu_parser> g_parser;

extern "C" {
uint32_t km_session_request_ocall(sgx_dh_msg1_t *dh_msg1, uint32_t *session_id);
uint32_t km_exchange_report_ocall(sgx_dh_msg2_t *dh_msg2,
                                  sgx_dh_msg3_t *dh_msg3, uint32_t session_id);
uint32_t km_send_request_ocall(uint32_t session_id,
                               secure_message_t *req_message,
                               size_t req_message_size, size_t max_payload_size,
                               secure_message_t *resp_message,
                               size_t resp_message_size);
uint32_t km_end_session_ocall(uint32_t session_id);

uint32_t next_data_batch(const uint8_t *data_hash, uint32_t hash_size,
                         uint8_t **data, uint32_t *len);

uint32_t ocall_get_frame(const char *ifs, uint32_t ifs_size, uint8_t **data,
                         uint32_t *len);
}

uint32_t km_session_request_ocall(sgx_dh_msg1_t *dh_msg1,
                                  uint32_t *session_id) {
  return g_parser->keymgr()->session_request(dh_msg1, session_id);
}
uint32_t km_exchange_report_ocall(sgx_dh_msg2_t *dh_msg2,
                                  sgx_dh_msg3_t *dh_msg3, uint32_t session_id) {
  return g_parser->keymgr()->exchange_report(dh_msg2, dh_msg3, session_id);
}
uint32_t km_send_request_ocall(uint32_t session_id,
                               secure_message_t *req_message,
                               size_t req_message_size, size_t max_payload_size,
                               secure_message_t *resp_message,
                               size_t resp_message_size) {
  return g_parser->keymgr()->generate_response(req_message, req_message_size,
                                               max_payload_size, resp_message,
                                               resp_message_size, session_id);
}
uint32_t km_end_session_ocall(uint32_t session_id) {
  return g_parser->keymgr()->end_session(session_id);
}

uint32_t next_data_batch(const uint8_t *data_hash, uint32_t hash_size,
                         uint8_t **data, uint32_t *len) {
  return g_parser->next_data_batch(data_hash, hash_size, data, len);
}

define_nt(video_frame, std::vector<unsigned char>);
typedef ::ff::util::ntobject<video_frame> video_frame_t;
define_nt(video_frames, std::vector<video_frame_t>);
typedef ::ff::util::ntobject<video_frames> video_frame_list_t;

int random_get_frame_index(int total_frames, int frame_capture_len) {
  std::vector<int> frame_index;
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, total_frames);
  return dist(rng);
}
uint32_t remove_file(const std::string &filename) {
  if (!boost::filesystem::exists(filename)) {
    LOG(ERROR) << "File does not exist";
    return 4;
  }
  try {
    boost::filesystem::remove(filename);
    LOG(INFO) << "File: " << filename << " removed successfully!";
  } catch (const boost::filesystem::filesystem_error &ex) {
    LOG(ERROR) << "Failed to remove file: " << ex.what();
    return 5;
  }
  return 0;
}
std::unique_ptr<uint8_t[]> g_mem_buf;

// 给定视频路径，随机截取其中一帧
uint32_t ocall_get_frame(const char *ifs, uint32_t ifs_size, uint8_t **data,
                         uint32_t *len) {
  cv::VideoCapture cap(ifs);
  // check if the file was opened successfully
  if (!cap.isOpened()) {
    LOG(ERROR) << "Error: Could not open the MP4 file.";
    return 1;
  }

  int total_frames = cap.get(cv::CAP_PROP_FRAME_COUNT); // 获取视频总帧数
  LOG(INFO) << "Frame count: " << total_frames;
  double fps = cap.get(cv::CAP_PROP_FPS); // 获取视频帧率
  double total_duration = total_frames / fps; // 计算视频总时长 = 视频总帧数 / 视频帧率
  LOG(INFO) << "Total time (duration) of the video: " << total_duration
            << " seconds";
  const int frame_capture_len = 1;
  int frame_index = random_get_frame_index(total_frames, frame_capture_len); // 随机选取一帧

  LOG(INFO) << "Frame index: " << frame_index;
  // read frame 
  cap.set(cv::CAP_PROP_POS_FRAMES, frame_index);
  cv::Mat frame; // 用于存储视频帧数据
  if (!cap.read(frame)) {
    LOG(ERROR) << "Failed to read frame";
    return 2;
  }

  // frame to bytes
  size_t frame_bytes_size = frame.total() * frame.elemSize(); // 计算帧数据的字节数
  LOG(INFO) << "Size of the frame in bytes: " << frame_bytes_size;
  std::vector<int> compression_params; // 压缩参数
  compression_params.push_back(cv::IMWRITE_JPEG_QUALITY); // 设置JPEG压缩质量
  const size_t max_frame_size = 1024 * 1024; // 设置最大帧数据大小为1MB
  size_t ratio =
      std::min(size_t(100), size_t(100.0 * max_frame_size / frame_bytes_size));
  LOG(INFO) << "compress ratio: " << ratio << "%";
  compression_params.push_back(ratio);
  std::vector<unsigned char> buf;
  bool ret = cv::imencode(".jpg", frame, buf, compression_params); // 将帧数据编码为JPEG格式
  
  // 检查是否编码成功
  if (!ret) {
    LOG(ERROR) << "Failed to encode frame to memory buffer";
    return 3;
  }

  // 将视频帧数据和时间戳打包为一个对象
  typename ypc::cast_obj_to_package<video_frame_t>::type pkg;
  pkg.set<::video_frame>(buf);
  pkg.set<::total_duration>(total_duration);
  pkg.set<::frame_ts>(total_duration * frame_index / total_frames);
  auto b = ypc::make_bytes<ypc::bytes>::for_package(pkg);
  LOG(INFO) << "serialized len: " << b.size();
  g_mem_buf = std::unique_ptr<uint8_t[]>(new uint8_t[b.size()]);
  memcpy(g_mem_buf.get(), b.data(), b.size());
  *data = g_mem_buf.get();
  *len = b.size();

  cap.release();
  return 0;
}
