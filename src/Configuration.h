#pragma once

#include <string>
#include <vector>

/**
 * Reads configuration from env variables
 */
class Configuration
{
public:
    Configuration();

    bool debug() const { return m_debug; }
    void debug(bool val) { m_debug = val; }

    bool verbose() const { return m_verbose; }
    void verbose(bool val) { m_verbose = val; }

    const std::string& base_url() const { return m_baseURL; }
    const std::string& base_dir() const { return m_baseDir; }
    const std::string& username() const { return m_username; }
    const std::string& password() const { return m_password; }
    size_t segment_duration() const { return m_segment_duration; }
    size_t max_backlog_total_segment_duration() const { return m_max_backlog_total_segment_duration; }

    const std::vector<std::string>& extensions() const { return m_extensions; }

private:
    bool                        m_debug = false;
    bool                        m_verbose = false;

    std::string                 m_baseURL;
    std::string                 m_baseDir;

    std::string                 m_username;
    std::string                 m_password;

    size_t                      m_segment_duration = 0;
    size_t                      m_max_backlog_total_segment_duration = 300;
    std::vector<std::string>    m_extensions = { ".ts" };
};
