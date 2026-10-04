// logger.hpp
// last updated: 04/10/2026
#pragma once
#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
namespace pk::core
{
    class logger
    {
    public:
        static logger &instance()
        {
            static logger s_inst;
            return s_inst;
        }
        void set_callback(std::function<void(const std::string &)> cb)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cb = cb;
        }
        const std::vector<std::string> &get_history()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_history;
        }
        static void log(const std::string &msg)
        {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            auto timer = std::chrono::system_clock::to_time_t(now);
            std::tm bt = *std::localtime(&timer);
            std::ostringstream oss;
            oss << "[" << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] " << msg;
            std::string formatted = oss.str();
            logger &inst = instance();
            std::lock_guard<std::mutex> lock(inst.m_mutex);
            inst.m_history.push_back(formatted);
            if (inst.m_history.size() > 5000)
                inst.m_history.erase(inst.m_history.begin());
            if (inst.m_cb)
                inst.m_cb(formatted);
        }

    private:
        logger() = default;
        ~logger() = default;
        logger(const logger &) = delete;
        logger &operator=(const logger &) = delete;

        std::mutex m_mutex;
        std::function<void(const std::string &)> m_cb;
        std::vector<std::string> m_history;
    };
}
namespace std
{
    template <typename T>
    inline std::string to_str(T &&val)
    {
        return std::to_string(std::forward<T>(val));
    }
}

// end