/*
 * Copyright (C) 2011-2012 IBM Corporation
 *
 * Author: Volker Schneider <volker.schneider@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "log.hpp"

#include "spdlog/sinks/null_sink.h"

namespace ffsp
{

static std::string s_logname;

void log_init(const std::string& logname, spdlog::level::level_enum level, const std::string& logfile /*= ""*/)
{
    auto logger = spdlog::get(logname);
    if (!logger)
    {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
        if (!logfile.empty())
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>(logfile, true));
        logger = std::make_shared<spdlog::logger>(logname, std::begin(sinks), std::end(sinks));
        logger->set_level(level);

        spdlog::register_logger(logger);
        s_logname = logname;
        logger->info("logger {} initialized", logname);
    }
    else
    {
        logger->warn("logger {} already initialized", logname);
    }
}

void log_uninit()
{
    log().info("logger {} about to be uninitialized", s_logname);
    spdlog::drop(s_logname);
}

spdlog::logger& log()
{
    auto logger = spdlog::get(s_logname);
    if (logger)
    {
        return *logger;
    }
    else
    {
        static auto null_logger{ std::make_shared<spdlog::logger>("null", std::make_shared<spdlog::sinks::null_sink_st>()) };
        return *null_logger;
    }
}

} // namespace ffsp
