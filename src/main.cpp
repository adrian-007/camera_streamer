#include <iostream>
#include <vector>
#include <regex>
#include <algorithm>
#include <chrono>
#include <thread>

#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "Configuration.h"
#include "CURLSession.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

/**
 * Search for files to upload.
 *
 * @param root path that will be searched for subdirectories matching a date format, containing target files
 * @param extensions of searched files
 * @return list of found files matching a location and extension patterns, ordered from newest to oldest
 */
std::deque<fs::path> find_upload_files(const fs::path& root, const std::vector<std::string>& extensions)
{
    const std::regex rx("[0-9]{4}-[0-9]{2}-[0-9]{2}");
    std::deque<fs::path> results;

    for (auto& dirEntry : fs::directory_iterator(root))
    {
        auto& p = dirEntry.path();

        if (std::regex_match(p.filename().string(), rx))
        {
            std::copy_if(fs::directory_iterator(p), fs::directory_iterator(), std::back_inserter(results), [&extensions](const fs::path& p)
            {
                auto ext = p.extension();
                return !ext.empty() && std::find(extensions.begin(), extensions.end(), ext.string()) != extensions.end();
            });
        }
    }

    // Sort in the reverse order, that is from newest to oldest
    std::sort(results.begin(), results.end(), std::greater<>());
    return results;
}


int main(int argc, char** argv)
{
    curl_global_init(CURL_GLOBAL_ALL);

    po::options_description desc("Options");
    desc.add_options()
        ("help", "Print help messages")
        ("verbose,v", "Make output verbose");

    po::variables_map vm;

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << argv[0] << ":" << std::endl << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    }
    catch (const po::error& e)
    {
        std::cerr << "Error: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;
        return 1;
    }

    int exitResult = 0;

    try
    {
        Configuration cfg;

        if (vm.count("verbose") || cfg.debug())
        {
            cfg.verbose(true);
        }

        if (cfg.base_dir().empty())
        {
            throw std::runtime_error("Base dir cannot be empty");
        }

        if (cfg.base_url().empty())
        {
            throw std::runtime_error("Base URL cannot be empty");
        }

        if (!cfg.username().empty() && cfg.password().empty())
        {
            throw std::runtime_error("Username provided without a password");
        }

        if (!cfg.segment_duration())
        {
            throw std::runtime_error("Segment duration not specified");
        }

        if (cfg.extensions().empty())
        {
            throw std::runtime_error("Extensions list cannot be empty");
        }

        if (!fs::exists(cfg.base_dir()) || !fs::is_directory(cfg.base_dir()))
        {
            throw std::runtime_error("Base directory doesn't exist or is not a directory");
        }

        std::cout << "Camera streamer is running" << std::endl;

        if (cfg.verbose())
        {
            std::cout << "* Base directory:      " << cfg.base_dir() << std::endl;
            std::cout << "* Base URL:            " << cfg.base_url() << std::endl;
            std::cout << "* Backlog duration:    " << cfg.max_backlog_total_segment_duration() << std::endl;
        }

        CURLSession session;

        // Enabling this will dump HTTP traffic to stdout
        session.set_verbose(cfg.debug());

        if (!cfg.username().empty() && !cfg.password().empty())
        {
            session.set_credentials(cfg.username(), cfg.password());
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

        while (true)
        {
            // First, look for files to upload
            auto paths = find_upload_files(cfg.base_dir(), cfg.extensions());

            if (paths.empty())
            {
                // If we didn't find any files, just sleep a bit and try again later
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            // We've found some files to upload.
            // First, we check a backlog - we'll calculate how many items do we have stored in storage, queued for sending
            // and if it's duration will exceed given threshold, we'll just delete them from filesystem. This algorithm
            // will ensure that once we're not ready for sending for a long period of time, we won't go out of storage
            // space. Default for backlog duration is 300 seconds, that is 5 minutes of video - can be changed by
            // environment variable.
            auto maxBacklogItems = cfg.max_backlog_total_segment_duration() / cfg.segment_duration();

            if (maxBacklogItems > 0)
            {
                while (paths.size() > maxBacklogItems)
                {
                    // Since order is from newest to oldest, pop items from the back - the oldest ones.
                    const auto& path = paths.back();

                    if (cfg.verbose())
                    {
                        std::cout << path.filename() << ": Removing from backlog" << std::endl;
                    }

                    fs::remove(path);

                    paths.pop_back();
                }
            }

            // Now it's time for file upload. Since we know for sure segment's duration, we can estimate how much
            // files from the backlog we can send until new segment arrives. In order to do that we'll keep track of
            // the time that we spent on each individual file, starting with a time pool equals to one segment duration
            // (in microseconds). After each upload, we'll subtract spent time from available time. Once available
            // time is negative, it means we need to refresh our upload file list. Minimal threshold for available
            // time is 10% of initial available time - we don't want to start sending next piece just before remaining
            // time is about to end.
            time_t availableUploadTime = cfg.segment_duration() * 1000000;
            time_t uploadTimeThreshold = availableUploadTime / 10;

            do
            {
                auto filePath = std::move(paths.front());
                paths.pop_front();

                auto fileSize = fs::file_size(filePath);
                time_t uploadTime = 0;

                if (cfg.verbose())
                {
                    std::cout << filePath.filename() << ": Uploading... ";
                    std::cout.flush();
                }

                if (!session.upload_file(filePath, fileSize, cfg.base_url(), uploadTime))
                {
                    if (cfg.debug())
                    {
                        std::cout << "Failed to upload file, breaking from loop." << std::endl;
                    }

                    break;
                }

                // Update time frame we have for uploading files from backlog.
                availableUploadTime -= uploadTime;

                if (cfg.verbose())
                {
                    auto elapsed = uploadTime / 1000000.0f;
                    auto transferSpeed = fileSize / elapsed / 1024.0f;

                    std::cout << "Done. Transfer speed: " << transferSpeed << " kB/s (" << fileSize << " bytes in " << elapsed << " second(s))" << std::endl;

                    if (cfg.debug())
                    {
                        if (availableUploadTime <= uploadTimeThreshold)
                        {
                            std::cout << "No enough time left for current window, refreshing upload file list" << std::endl;
                        }

                        std::cout << filePath.filename() << ": Removing file from disk... ";
                    }
                }

                // Since we've uploaded file to the server, we don't need it any more in the filesystem.
                boost::system::error_code systemError;
                if (!fs::remove(filePath, systemError))
                {
                    if (cfg.verbose())
                    {
                        std::cout << "Failed to remove uploaded file from disk (" << systemError << ")" << std::endl;
                    }
                }
                else
                {
                    if (cfg.debug())
                    {
                        std::cout << "Done" << std::endl;
                    }
                }

                // We'll continue looping until there are files in the backlog and we still have some time left.
            } while (!paths.empty() && availableUploadTime > uploadTimeThreshold);
        }

#pragma clang diagnostic pop

    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        exitResult = 1;
    }

    curl_global_cleanup();
    return exitResult;
}
