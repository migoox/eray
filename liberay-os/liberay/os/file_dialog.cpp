// Adapted from: https://github.com/gizmokis/resin/blob/master/resin/resin/dialog/file_dialog.hpp

#include <nfd/nfd.h>

#include <expected>
#include <liberay/os/file_dialog.hpp>
#include <liberay/util/path_utf8.hpp>
#include <nfd/nfd.hpp>

namespace eray::os {

FileDialog::~FileDialog() {
  if (is_active()) {
    // https://stackoverflow.com/a/58222149
    // The nfd is a blocking api so there is no way to tell it to stop working.
    // An alternative would be to freeze ui and wait for the dialog exit by calling .join() instead.
    this->dialog_thread_.detach();
  }
}

std::expected<void, FileDialog::FileDialogError> FileDialog::update() {
  if (!dialog_task_.has_value()) {
    return {};
  }

  if (dialog_task_->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    return {};
  }

  auto result = dialog_task_->get();
  if (result.has_value()) {
    if (on_finish_.has_value()) {
      auto path = util::utf8str_to_path(*result);

      if (std::filesystem::exists(path.parent_path())) {
        (*on_finish_)(std::move(path));
      } else {
        util::Logger::err("Incorrect path {} obtained from file dialog", path.string());
        return std::unexpected(DirectoryDoesNotExist);
      }
    } else {
      util::Logger::warn(R"(Obtained path target, but no handler is set)");
    }
  } else {
    util::Logger::info("File dialog cancelled");
  }

  dialog_task_ = std::nullopt;
  dialog_thread_.join();

  return {};
}

std::expected<void, FileDialog::FileDialogError> FileDialog::start_file_dialog(
    FileDialog::DialogType type, std::optional<std::span<const FilterItem>> filters,
    std::optional<std::string> default_name) {
  if (dialog_task_.has_value()) {
    util::Logger::warn("Detected an attempt to open second file dialog");
    return std::unexpected(FileDialogAlreadyOpen);
  }

  std::promise<std::optional<std::string>> curr_promise;
  dialog_task_   = curr_promise.get_future();
  dialog_thread_ = std::thread(
      [type](std::promise<std::optional<std::string>> promise,
             std::optional<std::span<const FilterItem>> _dialog_filters, std::optional<std::string> _default_name) {
        auto res = NFD_Init();
        if (res == NFD_ERROR) {
          util::Logger::err("Failed to open the file dialog");
        }

        nfdu8char_t* out_path = nullptr;
        nfdresult_t result    = NFD_ERROR;

        if (type == DialogType::OpenFile) {
          result = NFD_OpenDialogU8(
              &out_path,
              _dialog_filters ? reinterpret_cast<const nfdu8filteritem_t*>(_dialog_filters->data()) : nullptr,
              _dialog_filters ? static_cast<nfdfiltersize_t>(_dialog_filters->size()) : 0, nullptr);
        } else if (type == DialogType::SaveFile) {
          result = NFD_SaveDialogU8(
              &out_path,
              _dialog_filters ? reinterpret_cast<const nfdu8filteritem_t*>(_dialog_filters->data()) : nullptr,
              _dialog_filters ? static_cast<nfdfiltersize_t>(_dialog_filters->size()) : 0, nullptr,
              _default_name ? _default_name->data() : nullptr);
        } else {
          result = NFD_PickFolderU8(&out_path, nullptr);
        }

        if (result == NFD_OKAY) {
          promise.set_value(std::string(out_path));
          NFD_FreePathU8(out_path);
        } else {
          promise.set_value(std::nullopt);
        }

        NFD_Quit();
      },
      std::move(curr_promise), std::move(filters), std::move(default_name));
  return {};
}

}  // namespace eray::os
