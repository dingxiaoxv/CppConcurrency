#include <shared_mutex>
#include <unordered_map>
#include <thread>
#include <iostream>

class DnCache {
  private:
    mutable std::shared_mutex mtx_;
    std::unordered_map<std::string, std::string> entries_;

  public:
    std::string get(const std::string& domain) const {
      std::shared_lock<std::shared_mutex> lock(mtx_);
      auto it = entries_.find(domain);
      return it == entries_.end() ? "" : it->second;
    }

    void set(const std::string& domain, const std::string& ip) {
      std::lock_guard<std::shared_mutex> lock(mtx_);
      entries_[domain] = ip;
    }
    
};

int main() {
  DnCache cache;
  
  // 写入线程
  auto writer = [&cache]() {
    for(int i = 0; i < 5; i++) {
      std::string domain = "www.example" + std::to_string(i) + ".com";
      std::string ip = "192.168.1." + std::to_string(i);
      cache.set(domain, ip);
      std::cout << "Writer thread " << std::this_thread::get_id() 
                << " set " << domain << " -> " << ip << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };

  // 读取线程
  auto reader = [&cache]() {
    for(int i = 0; i < 5; i++) {
      std::string domain = "www.example" + std::to_string(i) + ".com";
      std::string ip = cache.get(domain);
      std::cout << "Reader thread " << std::this_thread::get_id() 
                << " got " << domain << " -> " << ip << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };

  // 创建并启动线程
  std::thread t1(writer);
  std::thread t2(reader);
  std::thread t3(reader);

  // 等待所有线程完成
  t1.join();
  t2.join();
  t3.join();

  return 0;
}