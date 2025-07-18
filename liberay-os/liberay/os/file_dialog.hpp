// Adapted from: https://github.com/gizmokis/resin/blob/master/resin/resin/dialog/file_dialog.hpp

#pragma once

#include <sys/types.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <future>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <optional>
#include <span>
#include <string>
#include <thread>

namespace eray::os {

class FileDialog {
 public:
  ERAY_DELETE_COPY_AND_MOVE(FileDialog)

  ~FileDialog();

  enum FileDialogError : uint8_t {
    DirectoryDoesNotExist = 0,
    FileDialogAlreadyOpen = 1,
  };

  // As the UTF8 version of the nfd is used on all platforms, the
  // following struct can be reinterpreted to match the nfdu8filteritem_t.
  struct FilterItem {
    FilterItem(const char* _name, const char* _spec) : name(_name), spec(_spec) {}

    const char* name;
    const char* spec;
  };

  static FileDialog& instance() {
    static FileDialog instance;
    return instance;
  }

  bool is_active() const { return dialog_task_.has_value(); }

  std::expected<void, FileDialogError> open_file(
      const std::function<void(const std::filesystem::path&)>& on_open_function,
      std::optional<std::span<const FilterItem>> filters = std::nullopt) {
    on_finish_ = on_open_function;
    return start_file_dialog(DialogType::OpenFile, std::move(filters));
  }

  std::expected<void, FileDialogError> save_file(
      const std::function<void(const std::filesystem::path&)>& on_save_function,
      std::optional<std::span<const FilterItem>> filters = std::nullopt,
      std::optional<std::string> default_name            = std::nullopt) {
    on_finish_ = on_save_function;
    return start_file_dialog(DialogType::SaveFile, std::move(filters), std::move(default_name));
  }

  std::expected<void, FileDialogError> pick_folder(
      const std::function<void(const std::filesystem::path&)>& on_pick_function) {
    on_finish_ = on_pick_function;
    return start_file_dialog(DialogType::PickFolder);
  }

  std::expected<void, FileDialogError> update();

 private:
  enum class DialogType : uint8_t { OpenFile = 0, SaveFile, PickFolder };

  FileDialog() = default;

  std::expected<void, FileDialogError> start_file_dialog(
      DialogType dialog_type, std::optional<std::span<const FilterItem>> filters = std::nullopt,
      std::optional<std::string> default_name = std::nullopt);

 private:
  std::optional<std::future<std::optional<std::string>>> dialog_task_;
  std::thread dialog_thread_;
  std::optional<std::function<void(const std::filesystem::path&)>> on_finish_;
};

}  // namespace eray::os
