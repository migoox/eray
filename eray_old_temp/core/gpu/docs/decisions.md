# Decisions

## Vulkan-HPP

* I decided to remove it.
* The main reason is that I don’t use exceptions. Although compiling Vulkan-HPP without exceptions is possible, the library uses assertions for all errors.
* There is a `resultCheck` function that relies on `VULKAN_HPP_ASSERT_ON_RESULT` (see below).
* The problem is that this approach prevents the caller from handling errors.
* For example, `vkQueuePresentKHR` may return `VK_ERROR_OUT_OF_DATE_KHR`, which can indicate that the window has been resized and the swapchain needs to be recreated. With `VULKAN_HPP_ASSERT_ON_RESULT`, this would simply trigger an assertion and crash the application instead of allowing it to handle the situation gracefully.
* The RAII layer is unnecessary, since my API already provides an abstraction layer between Vulkan and the user. The user is not intended to create resources directly through Vulkan.
* RAII will still be provided, but it will wrap the handles returned by my API instead.
* The only aspect I find useful is the type-safe enums. However, I plan to remove all Vulkan-specific types from the API layer, so they will not be exposed to the user in the future.
* An additional benefit of removing Vulkan-HPP is improved compile times and fewer indirections between the user call and the Vulkan layer.


```cpp
  namespace detail
  {
    VULKAN_HPP_INLINE void resultCheck( Result result, char const * message )
    {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
      ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
      ignore( message );
      VULKAN_HPP_ASSERT_ON_RESULT( result == Result::eSuccess );
#else
      if ( result != Result::eSuccess )
      {
        throwResultException( result, message );
      }
#endif
    }

    VULKAN_HPP_INLINE void resultCheck( Result result, char const * message, std::initializer_list<Result> successCodes )
    {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
      ignore( result );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
      ignore( message );
      ignore( successCodes );  // just in case VULKAN_HPP_ASSERT_ON_RESULT is empty
      VULKAN_HPP_ASSERT_ON_RESULT( std::find( successCodes.begin(), successCodes.end(), result ) != successCodes.end() );
#else
      if ( std::find( successCodes.begin(), successCodes.end(), result ) == successCodes.end() )
      {
        throwResultException( result, message );
      }
#endif
    }
  }  // namespace detail
```

## Handles

- My own handles were introduced to replace the Vk* handles
- Reason: In the future the library should be able to support dx12 
- Why not pointer to some struct? This way I abstract away what and how it is stored behind the GPU device layer.

## Error handling

- As most of the engines I dont use exceptions.
- Instead I've chosen to use `std::expected`. It is a new standard for error handling without exceptions.
- Higher levels will probably use nil values and only log the error, but I want the API layer to be solid and explicit about the errors and when they might happen.
