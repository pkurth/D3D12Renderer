#pragma once

#include <string>

std::string openFileDialog(const std::string& fileDescription, const std::string& extension);
std::string saveFileDialog(const std::string& fileDescription, const std::string& extension);
std::string directoryDialog();
