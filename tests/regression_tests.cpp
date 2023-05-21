#include <gtest/gtest.h>

#include "../src/main/main_processor.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void HandleMain(int argc, char *argv[],
                const std::optional<std::string> &pretty_print_file,
                const std::optional<std::string> &promela_file) {
  pid_t pid = fork();
  if (pid == 0) {
    FILE *file = freopen("output.txt", "w", stdout);
    FILE *file_err = freopen("output.txt", "w", stderr);

    std::ofstream file_std("output.txt");

    std::cout.rdbuf(file_std.rdbuf());
    std::streambuf *original_cout_buffer = std::cout.rdbuf();

    std::streambuf *origCin = std::cin.rdbuf();
    FILE *file_in = nullptr;

    if (pretty_print_file.has_value()) {
      std::string file_path = TEST_CASE_PATH + pretty_print_file.value();
      std::ifstream inputFile(TEST_CASE_PATH + pretty_print_file.value());
      std::cin.rdbuf(inputFile.rdbuf());
      file_err = freopen(file_path.c_str(), "r", stdin);
    }

    if (promela_file.has_value()) {
      std::string targetString = "file_path";
      std::string file_path = TEST_CASE_PATH + promela_file.value();
      for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == targetString) {
          char *new_arg = new char[file_path.length() + 1];
          std::strcpy(new_arg, file_path.c_str());
          argv[i] = new_arg;
          break;
        }
      }
    }

    MainProcessor main;
    main.main(argc, argv);

    std::cin.rdbuf(origCin);
    std::cout.rdbuf(original_cout_buffer);
    file_std.close();
    fclose(file);
    fclose(file_err);
    if (file_in) {
      fclose(file_in);
    }
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status)) << "Child process did not exit";
  } else {
    FAIL() << "Failed to fork child process";
  }
}

void AssertMain(const std::string &expected_result_file) {
  std::string file_path = TEST_CASE_PATH + expected_result_file;
  std::ifstream expected_file(file_path);
  std::ifstream output_file("output.txt");

  ASSERT_TRUE(expected_file.is_open())
      << "Failed to open expected result file: " << file_path;
  ASSERT_TRUE(output_file.is_open())
      << "Failed to open output file: output.txt";

  std::string expected_line;
  std::string output_line;
  bool mismatch = false;

  while (std::getline(expected_file, expected_line) &&
         std::getline(output_file, output_line)) {
    if (expected_line != output_line) {
      mismatch = true;
      break;
    }
  }

  expected_file.close();
  output_file.close();

  ASSERT_FALSE(mismatch) << "Output does not match expected result. Get: "
                         << output_line << " Expected: " << expected_line;
}

class AssertMainTest
    : public ::testing::TestWithParam<
          std::tuple<int, char **, std::string, std::optional<std::string>,
                     std::optional<std::string>>> {};

const char *pretty_print[] = {"", "-pp"};

INSTANTIATE_TEST_SUITE_P(
    ParamSet1, AssertMainTest,
    ::testing::Values(std::make_tuple(1, nullptr, "test_excpected_1",
                                      std::nullopt, std::nullopt),
                      std::make_tuple(2, const_cast<char **>(pretty_print),
                                      "test_excpected_2", "test_case_2",
                                      std::nullopt)));

TEST_P(AssertMainTest, BasicAssertions) {
  int param1;
  char **argv;
  std::string expected_result_file;
  std::optional<std::string> pretty_print_file;
  std::optional<std::string> file;
  std::tie(param1, argv, expected_result_file, pretty_print_file, file) =
      GetParam();
  HandleMain(param1, argv, pretty_print_file, file);
  AssertMain(expected_result_file);
}