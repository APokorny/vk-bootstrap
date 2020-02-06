#pragma once

#include <cassert>

#include <array>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vkb
{

namespace detail
{

enum class BootstrapErrorType : uint32_t
{

};

struct Error
{
	VkResult error_code;
	const char* msg;
};
template <typename E, typename U> class Expected
{
	public:
	Expected (const E& expect) : m_expect{ expect }, m_init{ true } {}
	Expected (E&& expect) : m_expect{ std::move (expect) }, m_init{ true } {}
	Expected (const Error& error) : m_error{ error }, m_init{ false } {}
	Expected (Error&& error) : m_error{ std::move (error) }, m_init{ false } {}
	~Expected () { destroy (); }
	Expected (Expected const& expected) : m_init (expected.m_init)
	{
		if (m_init)
			new (&m_expect) E{ expected.m_expect };
		else
			new (&m_error) Error{ expected.m_error };
	}
	Expected (Expected&& expected) : m_init (expected.m_init)
	{
		if (m_init)
			new (&m_expect) E{ std::move (expected.m_expect) };
		else
			new (&m_error) Error{ std::move (expected.m_error) };
		expected.destroy ();
	}

	Expected& operator= (const E& expect)
	{
		destroy ();
		m_init = true;
		new (&m_expect) E{ expect };
		return *this;
	}
	Expected& operator= (E&& expect)
	{
		destroy ();
		m_init = true;
		new (&m_expect) E{ std::move (expect) };
		return *this;
	}
	Expected& operator= (const Error& error)
	{
		destroy ();
		m_init = false;
		new (&m_error) Error{ error };
		return *this;
	}
	Expected& operator= (Error&& error)
	{
		destroy ();
		m_init = false;
		new (&m_error) Error{ std::move (error) };
		return *this;
	}
	// clang-format off
	const E* operator-> () const { assert (m_init); return &m_expect; }
	E*       operator-> ()       { assert (m_init); return &m_expect; }
	const E& operator* () const& { assert (m_init);	return m_expect; }
	E&       operator* () &      { assert (m_init); return m_expect; }
	E&&      operator* () &&	 { assert (m_init); return std::move (m_expect); }
	const E&  value () const&    { assert (m_init); return m_expect; }
	E&        value () &         { assert (m_init); return m_expect; }
	const E&& value () const&&   { assert (m_init); return std::move (m_expect); }
	E&&       value () &&        { assert (m_init); return std::move (m_expect); }
	const Error&  error () const&  { assert (!m_init); return m_error; }
	Error&        error () &       { assert (!m_init); return m_error; }
	const Error&& error () const&& { assert (!m_init); return std::move (m_error); }
	Error&&       error () &&      { assert (!m_init); return std::move (m_error); }
	// clang-format on
	bool has_value () const { return m_init; }
	explicit operator bool () const { return m_init; }

	private:
	void destroy ()
	{
		if (m_init)
			m_expect.~E ();
		else
			m_error.~Error ();
	}
	union {
		E m_expect;
		Error m_error;
	};
	bool m_init;
};

/* TODO implement operator == and operator != as friend or global */


// Helper for robustly executing the two-call pattern
template <typename T, typename F, typename... Ts>
auto get_vector_init (F&& f, T init, Ts&&... ts) -> Expected<std::vector<T>, VkResult>
{
	uint32_t count = 0;
	std::vector<T> results;
	VkResult err;
	do
	{
		err = f (ts..., &count, nullptr);
		if (err)
		{
			return Error{ err, "" };
		};
		results.resize (count, init);
		err = f (ts..., &count, results.data ());
	} while (err == VK_INCOMPLETE);
	if (err)
	{
		return Error{ err, "" };
	};
	return results;
}

template <typename T, typename F, typename... Ts>
auto get_vector (F&& f, Ts&&... ts) -> Expected<std::vector<T>, VkResult>
{
	return get_vector_init (f, T (), ts...);
}

template <typename T, typename F, typename... Ts>
auto get_vector_noerror (F&& f, T init, Ts&&... ts) -> std::vector<T>
{
	uint32_t count = 0;
	std::vector<T> results;
	f (ts..., &count, nullptr);
	results.resize (count, init);
	f (ts..., &count, results.data ());
	return results;
}
template <typename T, typename F, typename... Ts>
auto get_vector_noerror (F&& f, Ts&&... ts) -> std::vector<T>
{
	return get_vector_noerror (f, T (), ts...);
}

VkResult create_debug_utils_messenger (VkInstance instance,
    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback,
    VkDebugUtilsMessageSeverityFlagsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

void destroy_debug_utils_messenger (VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger);

static VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

bool check_layers_supported (std::vector<const char*> layer_names);

} // namespace detail

const char* to_string_message_severity (VkDebugUtilsMessageSeverityFlagBitsEXT s);
const char* to_string_message_type (VkDebugUtilsMessageTypeFlagsEXT s);

struct Instance
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
	bool headless = false;
	bool validation_enabled = false;
	bool debug_callback_enabled = false;
};

void destroy_instance (Instance instance); // release instance resources

class InstanceBuilder
{
	public:
	detail::Expected<Instance, VkResult> build (); // use builder pattern

	InstanceBuilder& set_app_name (std::string app_name);
	InstanceBuilder& set_engine_name (std::string engine_name);

	InstanceBuilder& set_app_version (uint32_t major, uint32_t minor, uint32_t patch);
	InstanceBuilder& set_engine_version (uint32_t major, uint32_t minor, uint32_t patch);
	InstanceBuilder& set_api_version (uint32_t major, uint32_t minor, uint32_t patch);

	InstanceBuilder& add_layer (std::string app_name);
	InstanceBuilder& add_extension (std::string app_name);

	InstanceBuilder& setup_validation_layers (bool enable_validation = true);
	InstanceBuilder& set_headless (bool headless = false);

	InstanceBuilder& set_default_debug_messenger ();
	InstanceBuilder& set_debug_callback (PFN_vkDebugUtilsMessengerCallbackEXT callback);
	InstanceBuilder& set_debug_messenger_severity (VkDebugUtilsMessageSeverityFlagsEXT severity);
	InstanceBuilder& add_debug_messenger_severity (VkDebugUtilsMessageSeverityFlagsEXT severity);
	InstanceBuilder& set_debug_messenger_type (VkDebugUtilsMessageTypeFlagsEXT type);
	InstanceBuilder& add_debug_messenger_type (VkDebugUtilsMessageTypeFlagsEXT type);

	InstanceBuilder& add_validation_disable (VkValidationCheckEXT check);
	InstanceBuilder& add_validation_feature_enable (VkValidationFeatureEnableEXT enable);
	InstanceBuilder& add_validation_feature_disable (VkValidationFeatureDisableEXT disable);

	private:
	struct InstanceInfo
	{
		// VkApplicationInfo
		std::string app_name;
		std::string engine_name;
		uint32_t application_version = 0;
		uint32_t engine_version = 0;
		uint32_t api_version = VK_MAKE_VERSION (1, 0, 0);

		// VkInstanceCreateInfo
		std::vector<std::string> layers;
		std::vector<std::string> extensions;
		VkInstanceCreateFlags flags = 0;
		std::vector<VkBaseOutStructure*> pNext_elements;

		// debug callback
		PFN_vkDebugUtilsMessengerCallbackEXT debug_callback = nullptr;
		VkDebugUtilsMessageSeverityFlagsEXT debug_message_severity =
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		VkDebugUtilsMessageTypeFlagsEXT debug_message_type =
		    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

		// validation features
		std::vector<VkValidationCheckEXT> disabled_validation_checks;
		std::vector<VkValidationFeatureEnableEXT> enabled_validation_features;
		std::vector<VkValidationFeatureDisableEXT> disabled_validation_features;

		// booleans
		bool ignore_non_critical_issues = true;
		bool enable_validation_layers = false;
		bool use_debug_messenger = false;
		bool headless_context = false;
	} info;
};

namespace detail
{

struct SurfaceSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

Expected<SurfaceSupportDetails, VkResult> query_surface_support_details (
    VkPhysicalDevice phys_device, VkSurfaceKHR surface);

struct QueueFamilies
{
	int graphics = -1;
	int present = -1;
	int transfer = -1;
	int compute = -1;
	int sparse = -1;
	uint32_t count_graphics = 0;
	uint32_t count_transfer = 0;
	uint32_t count_compute = 0;
	uint32_t count_sparse = 0;
};

VkFormat find_supported_format (VkPhysicalDevice physical_device,
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features);

std::vector<std::string> device_extension_support (VkPhysicalDevice device, std::vector<std::string> extensions);

detail::QueueFamilies find_queue_families (VkPhysicalDevice physDevice, VkSurfaceKHR windowSurface);

bool supports_features (VkPhysicalDeviceFeatures supported, VkPhysicalDeviceFeatures requested);

} // namespace detail

// ---- Physical Device ---- //

struct PhysicalDevice
{
	VkPhysicalDevice phys_device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkPhysicalDeviceFeatures physical_device_features{};
	VkPhysicalDeviceProperties physical_device_properties{};
	VkPhysicalDeviceMemoryProperties memory_properties{};

	detail::QueueFamilies queue_family_properties;

	std::vector<std::string> extensions_to_enable;
};

namespace detail
{
void populate_physical_device_details (PhysicalDevice physical_device);
} // namespace detail

struct PhysicalDeviceSelector
{
	public:
	PhysicalDeviceSelector (Instance instance);

	detail::Expected<PhysicalDevice, VkResult> select ();

	PhysicalDeviceSelector& set_surface (VkSurfaceKHR instance);

	PhysicalDeviceSelector& prefer_discrete (bool prefer_discrete = true);
	PhysicalDeviceSelector& prefer_integrated (bool prefer_integrated = true);
	PhysicalDeviceSelector& allow_fallback (bool fallback = true);

	PhysicalDeviceSelector& require_present (bool require = true);
	PhysicalDeviceSelector& require_dedicated_transfer_queue ();
	PhysicalDeviceSelector& require_dedicated_compute_queue ();

	PhysicalDeviceSelector& required_device_memory_size (VkDeviceSize size);
	PhysicalDeviceSelector& desired_device_memory_size (VkDeviceSize size);

	PhysicalDeviceSelector& add_required_extension (std::string extension);
	PhysicalDeviceSelector& add_desired_extension (std::string extension);

	PhysicalDeviceSelector& set_desired_version (uint32_t major, uint32_t minor);
	PhysicalDeviceSelector& set_minimum_version (uint32_t major = 1, uint32_t minor = 0);

	PhysicalDeviceSelector& set_required_features (VkPhysicalDeviceFeatures features);

	private:
	struct PhysicalDeviceInfo
	{
		VkInstance instance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		bool headless = false;
	} info;

	struct SelectionCriteria
	{
		bool prefer_discrete = true;
		bool prefer_integrated = false;
		bool allow_fallback = true;
		bool require_present = true;
		bool require_dedicated_transfer_queue = false;
		bool require_dedicated_compute_queue = false;
		VkDeviceSize required_mem_size = 0;
		VkDeviceSize desired_mem_size = 0;

		std::vector<std::string> required_extensions;
		std::vector<std::string> desired_extensions;

		uint32_t required_version = VK_MAKE_VERSION (1, 0, 0);
		uint32_t desired_version = VK_MAKE_VERSION (1, 0, 0);

		VkPhysicalDeviceFeatures required_features{};

	} criteria;

	enum class Suitable
	{
		yes,
		partial,
		no
	};

	Suitable is_device_suitable (VkPhysicalDevice phys_device);
};

// ---- Device ---- //

struct Device
{
	VkDevice device = VK_NULL_HANDLE;
	PhysicalDevice physical_device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
};

void destroy_device (Device device);

class DeviceBuilder
{
	public:
	DeviceBuilder (PhysicalDevice device);
	detail::Expected<Device, VkResult> build ();

	template <typename T> DeviceBuilder& add_pNext (T* structure);

	private:
	struct DeviceInfo
	{
		VkDeviceCreateFlags flags;
		std::vector<VkBaseOutStructure*> pNext_chain;
		PhysicalDevice physical_device;
		std::vector<std::string> extensions;
	} info;
};

// ---- Queue ---- //

namespace detail
{
VkQueue get_queue (Device const& device, uint32_t family, uint32_t index = 0);
}

uint32_t get_queue_index_present (Device const& device);
uint32_t get_queue_index_graphics (Device const& device);
uint32_t get_queue_index_compute (Device const& device);
uint32_t get_queue_index_transfer (Device const& device);
uint32_t get_queue_index_sparse (Device const& device);

detail::Expected<VkQueue, VkResult> get_queue_present (Device const& device);
detail::Expected<VkQueue, VkResult> get_queue_graphics (Device const& device, uint32_t index = 0);
detail::Expected<VkQueue, VkResult> get_queue_compute (Device const& device, uint32_t index = 0);
detail::Expected<VkQueue, VkResult> get_queue_transfer (Device const& device, uint32_t index = 0);
detail::Expected<VkQueue, VkResult> get_queue_sparse (Device const& device, uint32_t index = 0);


namespace detail
{
VkSurfaceFormatKHR find_surface_format (std::vector<VkFormat> const& available_formats);
VkPresentModeKHR find_present_mode (std::vector<VkPresentModeKHR> const& available_present_modes,
    std::vector<VkPresentModeKHR> const& desired_present_modes);
VkExtent2D find_extent (
    VkSurfaceCapabilitiesKHR const& capabilities, uint32_t desired_width, uint32_t desired_height);
} // namespace detail

struct Swapchain
{
	VkDevice device = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	uint32_t image_count = 0;
	VkFormat image_format = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = { 0, 0 };
};

void destroy_swapchain (Swapchain const& swapchain);
detail::Expected<std::vector<VkImage>, VkResult> get_swapchain_images (Swapchain const& swapchain);
detail::Expected<std::vector<VkImageView>, VkResult> get_swapchain_image_views (
    Swapchain const& swapchain, std::vector<VkImage> const& images);

class SwapchainBuilder
{
	public:
	SwapchainBuilder (Device const& device);
	SwapchainBuilder (VkPhysicalDevice const physical_device, VkDevice const device, VkSurfaceKHR const surface);

	detail::Expected<Swapchain, VkResult> build ();
	detail::Expected<Swapchain, VkResult> recreate (Swapchain const& swapchain);

	// SwapchainBuilder& set_desired_image_count (uint32_t count);
	// SwapchainBuilder& set_maximum_image_count (uint32_t count);

	SwapchainBuilder& set_desired_format (VkSurfaceFormatKHR format);
	SwapchainBuilder& add_fallback_format (VkSurfaceFormatKHR format);
	SwapchainBuilder& use_default_format_selection ();

	SwapchainBuilder& set_desired_present_mode (VkPresentModeKHR present_mode);
	SwapchainBuilder& add_fallback_present_mode (VkPresentModeKHR present_mode);
	SwapchainBuilder& use_default_present_mode_selection ();

	private:
	struct SwapchainInfo
	{
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
		std::vector<VkSurfaceFormatKHR> desired_formats;
		std::vector<VkPresentModeKHR> desired_present_modes;
		uint32_t desired_width = 256;
		uint32_t desired_height = 256;
	} info;
};

} // namespace vkb