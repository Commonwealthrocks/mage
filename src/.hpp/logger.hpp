// logger.hpp
// last updated: 30/10/2026
#pragma once
#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <chrono>
#include <ctime>
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
            m_cb = std::move(cb);
        }
        std::vector<std::string> get_history()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_history;
        }
        static void log(const std::string &msg)
        {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            if (ms.count() < 0)
                ms = std::chrono::milliseconds(0);
            std::time_t timer = std::chrono::system_clock::to_time_t(now);
            std::tm bt{};
#ifdef _WIN32
            localtime_s(&bt, &timer);
#else
            localtime_r(&timer, &bt);
#endif
            std::ostringstream oss;
            oss << "[" << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] ";
            for (unsigned char ch : msg)
            {
                if (ch == '\n' || ch == '\r')
                    oss << ' ';
                else if (ch < 0x20 || ch == 0x7F)
                    oss << '?';
                else
                    oss << static_cast<char>(ch);
            }
            std::string formatted = oss.str();
            std::function<void(const std::string &)> cb_copy;
            {
                logger &inst = instance();
                std::lock_guard<std::mutex> lock(inst.m_mutex);
                inst.m_history.push_back(formatted);
                if (inst.m_history.size() > 5000)
                    inst.m_history.erase(inst.m_history.begin());
                cb_copy = inst.m_cb;
            }
            if (cb_copy)
                cb_copy(formatted);
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

// end
