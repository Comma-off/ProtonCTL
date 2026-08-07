#include "protonctl/process_util.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>

namespace protonctl {

ProcessResult ProcessUtil::Run(const std::vector<std::string>& argv, const fs::path& working_dir,
                                const LineCallback& on_line) {
  if (argv.empty()) throw ProtonCtlError("ProcessUtil::Run called with empty argv");

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    throw ProtonCtlError(std::string("pipe() failed: ") + std::strerror(errno));
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    throw ProtonCtlError(std::string("fork() failed: ") + std::strerror(errno));
  }

  if (pid == 0) {
    // Child: redirect stdout+stderr into the write end of the pipe.
    close(pipe_fds[0]);
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);

    std::error_code ec;
    if (!working_dir.empty()) {
      if (chdir(working_dir.c_str()) != 0) {
        _exit(127);
      }
    }

    std::vector<char*> c_argv;
    c_argv.reserve(argv.size() + 1);
    for (const auto& a : argv) c_argv.push_back(const_cast<char*>(a.c_str()));
    c_argv.push_back(nullptr);

    execvp(c_argv[0], c_argv.data());
    _exit(127);  // execvp only returns on failure
  }

  // Parent.
  close(pipe_fds[1]);

  ProcessResult result;
  std::string line_buffer;
  std::array<char, 4096> chunk{};

  ssize_t n;
  while ((n = read(pipe_fds[0], chunk.data(), chunk.size())) > 0) {
    result.combined_output.append(chunk.data(), static_cast<size_t>(n));
    if (on_line) {
      line_buffer.append(chunk.data(), static_cast<size_t>(n));
      size_t pos;
      while ((pos = line_buffer.find('\n')) != std::string::npos) {
        on_line(line_buffer.substr(0, pos));
        line_buffer.erase(0, pos + 1);
      }
    }
  }
  close(pipe_fds[0]);

  if (on_line && !line_buffer.empty()) on_line(line_buffer);

  int status = 0;
  waitpid(pid, &status, 0);
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return result;
}

}  // namespace protonctl
