/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 *
 *    This file is part of autotier.
 *
 *    autotier is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    autotier is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "alert.hpp"
#include "config.hpp"
#include "tools.hpp"
#include "version.hpp"

#include <45d/config/ConfigParser.hpp>
#include <45d/socket/UnixSocketClient.hpp>
#include <sstream>
#include <thread>

extern "C" {
#include <getopt.h>
}

fs::path get_run_path(const fs::path &config_path) {
	try {
		// try Global
		fs::path run_path = ffd::ConfigParser(config_path.string())
								.get_from<fs::path>("Global", "Run Path", "/var/lib/autotier");
		return run_path / std::to_string(std::hash<std::string>{}(config_path.string()));
	} catch (const ffd::NoConfigException &e) {
		Logging::log.error(e.what());
		exit(EXIT_FAILURE);
	} catch (const std::out_of_range &) {
		try {
			// try global
			fs::path run_path = ffd::ConfigParser(config_path.string())
									.get_from<fs::path>("global", "Run Path", "/var/lib/autotier");
			return run_path / std::to_string(std::hash<std::string>{}(config_path.string()));
		} catch (const std::out_of_range &) {
			// try top level scope
			fs::path run_path = ffd::ConfigParser(config_path.string())
									.get<fs::path>("Run Path", "/var/lib/autotier");
			return run_path / std::to_string(std::hash<std::string>{}(config_path.string()));
		}
	}
}

int main(int argc, char *argv[]) {
	int opt;
	int option_ind = 0;
	int cmd;
	bool print_version = false;
	bool json = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;

	static struct option long_options[] = { { "config", required_argument, 0, 'c' },
											{ "help", no_argument, 0, 'h' },
											{ "json", no_argument, 0, 'j' },
											{ "verbose", no_argument, 0, 'v' },
											{ "quiet", no_argument, 0, 'q' },
											{ "version", no_argument, 0, 'V' },
											{ 0, 0, 0, 0 } };

	/* Get CLI options.
	 */
	while ((opt = getopt_long(argc, argv, "c:hjvqV", long_options, &option_ind)) != -1) {
		switch (opt) {
			case 'c':
				config_path = optarg;
				break;
			case 'h':
				cli_usage();
				exit(EXIT_SUCCESS);
			case 'j':
				json = true;
				break;
			case 'v':
				Logging::log.set_level(Logger::log_level_t::DEBUG);
				break;
			case 'q':
				Logging::log.set_level(Logger::log_level_t::NONE);
				break;
			case 'V':
				print_version = true;
				break;
			case '?':
				break; // getopt_long prints errors
			default:
				abort();
		}
	}

	if (print_version) {
		Logging::log.message("autotier " VERS, Logger::log_level_t::NONE);
		Logging::log.message(
			u8"   ┓\n"
			u8"└─ ┃ ├─\n"
			u8"└─ ┣ ├─\n"
			u8"└─ ┃ └─\n"
			u8"   ┛",
			Logger::log_level_t::NORMAL);
		Logging::log.message(
			"The logo shows three separate tiers on the left being combined into one storage space on the right.\n"
			"The use of "
			u8"└─"
			" to represent filesystem hierarchy was inspired by the output of `tree`.",
			Logger::log_level_t::DEBUG);
		exit(EXIT_SUCCESS);
	}

	/* Grab command.
	 */
	if (optind < argc) {
		cmd = get_command_index(argv[optind]);
	} else {
		Logging::log.error("No command passed.");
		cli_usage();
		exit(EXIT_FAILURE);
	}

	if (cmd == -1) {
		Logging::log.error("Invalid command: " + std::string(argv[optind]));
		cli_usage();
		exit(EXIT_FAILURE);
	}

	/* Process ad hoc command.
	 */
	if (cmd == HELP) {
		cli_usage();
	} else {
		std::vector<std::string> payload;
		payload.push_back(argv[optind++]); // push command name
		if (cmd == PIN) {
			if (optind == argc) {
				Logging::log.error("No arguments passed.");
				exit(EXIT_FAILURE);
			}
			payload.push_back(argv[optind++]); // push tier name
		}
		if (cmd == PIN || cmd == UNPIN || cmd == WHICHTIER) {
			std::list<std::string> paths;
			while (optind < argc)
				paths.push_back(argv[optind++]);
			if (paths.empty()) {
				Logging::log.error("No arguments passed.");
				exit(EXIT_FAILURE);
			}
			sanitize_paths(paths);
			if (paths.empty()) {
				Logging::log.error("No remaining valid paths.");
				exit(EXIT_FAILURE);
			}
			for (const std::string &path : paths)
				payload.push_back(path);
		} else if (cmd == STATUS) {
			std::stringstream ss;
			ss << std::boolalpha << json << std::endl;
			payload.push_back(ss.str());
		}

		fs::path run_path = get_run_path(config_path);
		if (run_path == "") {
			Logging::log.error("Could not find metadata path.");
			exit(EXIT_FAILURE);
		}

		std::vector<std::string> response;
		try {
			ffd::UnixSocketClient socket_client((run_path / "adhoc.socket").string());
			socket_client.connect();
			socket_client.send_data_async(payload);
			socket_client.receive_data_async(response);
			socket_client.close_connection();
		} catch (const ffd::SocketWriteException &e) {
			Logging::log.error(std::string("Socket request error: ") + e.what());
			exit(EXIT_FAILURE);
		} catch (const ffd::SocketReadException &e) {
			Logging::log.error(std::string("Socket reply error: ") + e.what());
			exit(EXIT_FAILURE);
		} catch (const ffd::SocketConnectException &e) {
			switch (e.get_errno()) {
				case ECONNREFUSED:
					Logging::log.error("Socket connection refused. Is autotier mounted?");
					break;
				case EACCES:
					Logging::log.error(
						"Permission denied. Run as root or a member of group `autotier`.");
					break;
				default:
					Logging::log.error(std::string("Socket connect error: ") + e.what());
					break;
			}
			exit(EXIT_FAILURE);
		} catch (const ffd::SocketException &e) {
			Logging::log.error(std::string("Socket error: ") + e.what());
			exit(EXIT_FAILURE);
		}

		if (response.front() == "OK") {
			Logging::log.message("Response OK.", Logger::log_level_t::DEBUG);
			for (std::vector<std::string>::iterator itr = std::next(response.begin());
				 itr != response.end();
				 ++itr) {
				Logging::log.message(*itr, Logger::log_level_t::NONE);
			}
		} else {
			for (std::vector<std::string>::iterator itr = std::next(response.begin());
				 itr != response.end();
				 ++itr) {
				Logging::log.error(*itr);
			}
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
