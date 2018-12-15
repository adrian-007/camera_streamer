#include <cstdlib>
#include <cstring>

#include "Configuration.h"

Configuration::Configuration()
{
    char* pValue;

    if ((pValue = getenv("CAMERA_DEBUG")) != nullptr)
    {
        m_debug = strlen(pValue) == 0 || strncasecmp(pValue, "true", 4) == 0 || strtol(pValue, nullptr, 0) != 0;
    }

    if ((pValue = getenv("CAMERA_BASE_URL")) != nullptr)
    {
        m_baseURL = pValue;
    }

    if ((pValue = getenv("CAMERA_BASE_DIR")) != nullptr)
    {
        m_baseDir = pValue;
    }

    if ((pValue = getenv("CAMERA_USERNAME")) != nullptr)
    {
        m_username = pValue;
    }

    if ((pValue = getenv("CAMERA_PASSWORD")) != nullptr)
    {
        m_password = pValue;
    }

    if ((pValue = getenv("CAMERA_SEGMENT_DURATION")) != nullptr)
    {
        m_segment_duration = static_cast<size_t>(strtoll(pValue, nullptr, 10));
    }

    if ((pValue = getenv("CAMERA_MAX_BACKLOG_TOTAL_SEGMENT_DURATION")) != nullptr)
    {
        m_max_backlog_total_segment_duration = static_cast<size_t>(strtoll(pValue, nullptr, 10));
    }

    // It makes sense to keep at least one file around...
    if (m_max_backlog_total_segment_duration > 0 && m_max_backlog_total_segment_duration < m_segment_duration)
    {
        m_max_backlog_total_segment_duration = m_segment_duration;
    }
}
