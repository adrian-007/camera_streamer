#include "CURLSession.h"

#include <iostream>

#include <curl/curl.h>
#include <boost/filesystem.hpp>


CURLSession::CURLSession()
    : m_handle { curl_easy_init(), &curl_easy_cleanup }
{
}


bool CURLSession::is_valid() const
{
    return m_handle != nullptr;
}


void CURLSession::set_verbose(bool verbose)
{
    assert(m_handle != nullptr);
    curl_easy_setopt(m_handle.get(), CURLOPT_VERBOSE, verbose ? 1L : 0L);
}


void CURLSession::set_credentials(const std::string& username, const std::string& password)
{
    assert(m_handle != nullptr);

    curl_easy_setopt(m_handle.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(m_handle.get(), CURLOPT_USERNAME, username.c_str());
    curl_easy_setopt(m_handle.get(), CURLOPT_PASSWORD, password.c_str());
}


bool CURLSession::upload_file(const boost::filesystem::path& filePath, size_t fileSize, const std::string& rootURL, time_t& uploadTime)
{
    assert(m_handle != nullptr);

    std::unique_ptr<FILE, int(*)(FILE*)> filePtr { fopen(filePath.c_str(), "rb"), &fclose };

    if (filePtr == nullptr)
    {
        std::cerr << filePath << ": Cannot open file (" << errno << ")" << std::endl;
        return false;
    }

    auto targetURL = rootURL + "/" + escape_string(filePath.parent_path().filename().string()).get() + "/" + escape_string(filePath.filename().string()).get();

    curl_easy_setopt(m_handle.get(), CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_handle.get(), CURLOPT_PUT, 1L);
    curl_easy_setopt(m_handle.get(), CURLOPT_URL, targetURL.c_str());
    curl_easy_setopt(m_handle.get(), CURLOPT_READDATA, filePtr.get());
    curl_easy_setopt(m_handle.get(), CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(fileSize));

    auto res = curl_easy_perform(m_handle.get());
    if (res != CURLE_OK)
    {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    curl_easy_getinfo(m_handle.get(), CURLINFO_TOTAL_TIME_T, &uploadTime);

    return true;
}


std::unique_ptr<char, void(*)(void*)> CURLSession::escape_string(const std::string& str) const
{
    return std::unique_ptr<char, void(*)(void*)>(curl_easy_escape(m_handle.get(), str.c_str(), static_cast<int>(str.size())), &curl_free);
}
