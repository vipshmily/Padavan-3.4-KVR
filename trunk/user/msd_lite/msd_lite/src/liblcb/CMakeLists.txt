#
# Cmake configuration file for liblcb.
# Just add: include(liblcb/CMakeLists.txt) in CMakeLists.txt to use this lib.
#

############################# INITIAL SECTION ##########################
cmake_minimum_required(VERSION 3.10)

############################# OPTIONS SECTION ##########################


############################# INCLUDE SECTION ##########################
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)

############################# MACRO SECTION ############################
macro(chk_include_files incfile prop)
	string(TOUPPER HAVE_${prop} __tmp)
	check_include_files(${incfile} ${__tmp})
	if (${__tmp})
		add_definitions(-D${__tmp})
	endif()
endmacro()

macro(chk_function_exists func)
	string(TOUPPER HAVE_${func} __tmp)
	check_function_exists(${func} ${__tmp})
	if (${__tmp})
		add_definitions(-D${__tmp})
	endif()
endmacro()

macro(chk_symbol_exists incfile symbol)
	string(TOUPPER HAVE_${symbol} __tmp)
	check_symbol_exists(${symbol} ${incfile} ${__tmp})
	if (${__tmp})
		add_definitions(-D${__tmp})
	endif()
endmacro()

############################# CONFIG SECTION ###########################
# Prefer local include dirs to system ones.
include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

message(STATUS "liblcb configuring...")


# Platform specific configuration.
if (CMAKE_SYSTEM_NAME MATCHES "^.*BSD$|DragonFly")
	add_definitions(-D_BSD_SOURCE -DFREEBSD)
	include_directories("/usr/local/include/")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	add_definitions(-D_BSD_SOURCE -DDARWIN)
endif()
if (APPLE)
	# For IPV6_PKTINFO.
	add_definitions(-D__APPLE_USE_RFC_3542)
endif()


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(-D_GNU_SOURCE -DLINUX -D__USE_GNU=1)
	if (BUILD_CPU_MODE STREQUAL "32")
		add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
	endif()
	list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
endif()


# Check platform specific includes.
#chk_include_files(sys/types.h SYS_TYPES_H)

# Check platform API.
chk_function_exists(explicit_bzero)
chk_function_exists(memrchr)
chk_function_exists(memmem)
chk_function_exists(strlcpy)
chk_function_exists(strncasecmp)
chk_function_exists(reallocarray)
chk_function_exists(freezero)
chk_function_exists(pipe2)
chk_function_exists(accept4)
chk_function_exists(rtprio)

# Check macros.
chk_symbol_exists(sys/socket.h SOCK_NONBLOCK)

# Disable some warnings.
try_c_flag(W_NO_UNUSED_RESULT		"-Wno-unused-result")


message(STATUS "liblcb configuring done!")

################################ SUBDIRS SECTION #######################


############################ TARGETS SECTION ###########################


##################### INSTALLATION #####################################

