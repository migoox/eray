#pragma once
#include <concepts>
#include <functional>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <ranges>
#include <utility>
#include <variant>

namespace eray::os {

enum class WindowEventType : uint8_t {
  WindowClosed        = 0,
  WindowResized       = 1,
  WindowFocused       = 2,
  WindowLostFocus     = 3,
  WindowMoved         = 4,
  KeyPressed          = 5,
  KeyReleased         = 6,
  MouseButtonPressed  = 7,
  MouseButtonReleased = 8,
  MouseScrolled       = 9,
  MouseEntered        = 10,
  MouseLeft           = 11,
  FramebufferResized  = 12,
  _Count              = 13,  // NOLINT
};

constexpr std::size_t kWindowEventCount = static_cast<std::size_t>(WindowEventType::_Count);

constexpr auto kWindowEventTypeName = util::StringEnumMapper<WindowEventType>({
    {WindowEventType::WindowClosed, "WindowClosedEvent"},
    {WindowEventType::WindowResized, "WindowResizedEvent"},
    {WindowEventType::WindowFocused, "WindowFocusedEvent"},
    {WindowEventType::WindowLostFocus, "WindowLostFocusEvent"},
    {WindowEventType::WindowMoved, "WindowMovedEvent"},
    {WindowEventType::KeyPressed, "KeyPressedEvent"},
    {WindowEventType::KeyReleased, "KeyReleasedEvent"},
    {WindowEventType::MouseButtonPressed, "MouseButtonPressedEvent"},
    {WindowEventType::MouseButtonReleased, "MouseButtonReleasedEvent"},
    {WindowEventType::MouseScrolled, "MouseScrolledEvent"},
    {WindowEventType::MouseEntered, "MouseEnteredEvent"},
    {WindowEventType::MouseLeft, "MouseLeftEvent"},
    {WindowEventType::FramebufferResized, "FramebufferResized"},
});

template <typename T>
concept CWindowEvent = requires(T t) {
  { T::name() } -> std::same_as<util::zstring_view>;
  { T::type() } -> std::same_as<WindowEventType>;
};

/**
 * @brief Base class of any `WindowEvent` class. It provides compile-time (static) polymorphism.
 *
 * @tparam EventType
 */
template <WindowEventType TEventType>
class WindowEventBase {
 public:
  static constexpr util::zstring_view name() { return kWindowEventTypeName[TEventType]; }
  static constexpr WindowEventType type() { return TEventType; }
};

class WindowClosedEvent : public WindowEventBase<WindowEventType::WindowClosed> {
 public:
  WindowClosedEvent() = default;
  std::string to_string() { return std::string(name()); }  // NOLINT
};

class WindowResizedEvent : public WindowEventBase<WindowEventType::WindowResized> {
 public:
  explicit WindowResizedEvent(int width, int height) : width_(width), height_(height) {}
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  int width_{};
  int height_{};
};

class WindowFocusedEvent : public WindowEventBase<WindowEventType::WindowFocused> {
 public:
  WindowFocusedEvent() = default;
};

class WindowLostFocusEvent : public WindowEventBase<WindowEventType::WindowLostFocus> {
 public:
  WindowLostFocusEvent() = default;
};

class WindowMovedEvent : public WindowEventBase<WindowEventType::WindowMoved> {
 public:
  explicit WindowMovedEvent(int x, int y) : x_(x), y_(y) {}
  int x() const { return x_; }
  int y() const { return y_; }

 private:
  int x_{};
  int y_{};
};

class KeyPressedEvent : public WindowEventBase<WindowEventType::KeyPressed> {
 public:
  explicit KeyPressedEvent(KeyCode key_code) : key_code_(key_code) {}
  KeyCode key_code() const { return key_code_; }

 private:
  KeyCode key_code_{};
};

class KeyReleasedEvent : public WindowEventBase<WindowEventType::KeyReleased> {
 public:
  explicit KeyReleasedEvent(KeyCode key_code) : key_code_(key_code) {}
  KeyCode key_code() const { return key_code_; }

 private:
  KeyCode key_code_{};
};

class MouseButtonPressedEvent : public WindowEventBase<WindowEventType::MouseButtonPressed> {
 public:
  MouseButtonPressedEvent(MouseBtnCode mouse_btn_code_, double x, double y)
      : mouse_btn_code_(mouse_btn_code_), x_(x), y_(y) {}
  MouseBtnCode mouse_btn_code() const { return mouse_btn_code_; }
  double x() const { return x_; }
  double y() const { return y_; }

 private:
  MouseBtnCode mouse_btn_code_{};
  double x_{};
  double y_{};
};

class MouseButtonReleasedEvent : public WindowEventBase<WindowEventType::MouseButtonReleased> {
 public:
  explicit MouseButtonReleasedEvent(MouseBtnCode mouse_btn_code, double x, double y)
      : mouse_btn_code_(mouse_btn_code), x_(x), y_(y) {}
  double x() const { return x_; }
  double y() const { return y_; }
  bool is_on_ui() const { return on_ui_; }

  MouseBtnCode mouse_btn_code() const { return mouse_btn_code_; }

 private:
  MouseBtnCode mouse_btn_code_{};
  double x_{};
  double y_{};
  bool on_ui_{};
};

class MouseScrolledEvent : public WindowEventBase<WindowEventType::MouseScrolled> {
 public:
  explicit MouseScrolledEvent(double x_offset, double y_offset) : x_offset_(x_offset), y_offset_(y_offset) {}
  double x_offset() const { return x_offset_; }
  double y_offset() const { return y_offset_; }

 private:
  double x_offset_{};
  double y_offset_{};
};

class MouseEntered : public WindowEventBase<WindowEventType::MouseEntered> {
 public:
  MouseEntered() = default;
};

class MouseLeft : public WindowEventBase<WindowEventType::MouseLeft> {
 public:
  MouseLeft() = default;
};

class FramebufferResizedEvent : public WindowEventBase<WindowEventType::FramebufferResized> {
 public:
  FramebufferResizedEvent() = default;
};

template <CWindowEvent TEvent>
using EventCallback = std::function<bool(const TEvent&)>;

/**
 * @brief Allows for converting a class method to an event callback. It creates a lambda wrapper with reference to an
 * object and it's method.
 *
 * @warning The event dispatcher must not outlive the provided class instance. For that reason a typical use case
 * is when the class instance is an owner of the dispatcher (not necessarily a direct one).
 *
 * @tparam TClass
 * @tparam TEvent
 * @param method
 * @return constexpr auto
 */
template <typename TClass, typename TMethod>
static constexpr auto class_method_as_event_callback(TClass* obj, TMethod TClass::* method) {
  // TODO(migoox): this function should show an error when method with improper signature is provided before the
  // actual explicit compilation
  return [obj, method](auto&& arg) -> bool { return (obj->*method)(std::forward<decltype(arg)>(arg)); };
}

/**
 * @brief This class allows for subscribing/dispatching window events that are specified in the pack.
 * The dispatcher instance is owned by `Window` class.
 *
 * @tparam TEvents
 */
template <CWindowEvent... TEvents>
class WindowEventDispatcherBase {
 public:
  template <CWindowEvent TEvent>
  using CallbacksFor = std::vector<EventCallback<TEvent>>;

  /**
   * @brief Subscribe to event notifications dispatched by window event dispatcher. It's
   * possible to set multiple event callbacks to one event. The last added callback will be invoked first.
   *
   * @tparam TEvent
   * @param callback_fn
   */
  template <CWindowEvent TEvent>
  void set_event_callback(const EventCallback<TEvent>& callback_fn) {
    std::get<CallbacksFor<TEvent>>(subscribers_).push_back(callback_fn);
  }

  /**
   * @brief Dispatches all the deferred (enqueued) events and clears the queue. It's blocking operation that is
   * typically invoked at the begining of the game loop.
   *
   */
  void process_queued_events() {
    for (auto& event : events_queue_) {
      std::visit(WindowEventVisitor{*this}, event);
    }
    events_queue_.clear();
  }

  /**
   * @brief Deferrs event dispatching to the moment when `flush_deferred_events` is called (non-blocking).
   *
   * @param event
   */
  template <CWindowEvent TEvent>
  void enqueue_event(const TEvent& event) {
    events_queue_.push_back(event);
  }

  /**
   * @brief Dispatches the provided event immediately (blocking).
   *
   * @tparam TEvent
   * @param callback_fn
   */
  template <CWindowEvent TEvent>
  void dispatch_event(const TEvent& event) {
    auto& callbacks = std::get<CallbacksFor<TEvent>>(subscribers_);
    for (auto& subscriber : callbacks | std::ranges::views::reverse) {
      subscriber(event);
    }
  }

 private:
  struct WindowEventVisitor {
    WindowEventDispatcherBase<TEvents...>& dispatcher;  // NOLINT

    template <typename TEvent>
    void operator()(const TEvent& event) const {
      dispatcher.dispatch_event(event);
    }
  };

 private:
  std::tuple<std::vector<EventCallback<TEvents>>...> subscribers_{};
  std::vector<std::variant<TEvents...>> events_queue_{};
};

/**
 * @brief This class allows for subscribing/dispatching all window events.
 *
 */
using WindowEventDispatcher =
    WindowEventDispatcherBase<KeyPressedEvent, KeyReleasedEvent, MouseButtonPressedEvent, MouseButtonReleasedEvent,
                              WindowClosedEvent, WindowResizedEvent, WindowFocusedEvent, WindowLostFocusEvent,
                              WindowMovedEvent, MouseScrolledEvent, MouseEntered, MouseLeft, FramebufferResizedEvent>;

}  // namespace eray::os
