#pragma once

#include <memory>

namespace boost
{
    namespace filesystem
    {
        class path;
    }
}

typedef void CURL;

class CURLSession
{
public:
    CURLSession();

    bool is_valid() const;

    void set_verbose(bool verbose);

    void set_credentials(const std::string& username, const std::string& password);

    bool upload_file(const boost::filesystem::path& filePath, size_t fileSize, const std::string& rootURL, time_t& uploadTime);

private:
    std::unique_ptr<CURL, void(*)(CURL*)> m_handle;

    std::unique_ptr<char, void(*)(void*)> escape_string(const std::string& str) const;
};
