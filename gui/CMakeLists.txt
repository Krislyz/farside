
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
	enable_language(OBJCXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent Svg Qml Quick Widgets Network)
# Farside: QR codes for pairing (built by play/build-play.ps1).
find_package(ZXing REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.10.0")
	find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()

find_package(Qt6 COMPONENTS WebEngineQuick)
find_package(Qt6 COMPONENTS DBus)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

set(SOURCE_FILES
	include/exception.h
	src/main.cpp
	include/farsidelink.h
	src/farsidelink.cpp
	include/farsidesession.h
	src/farsidesession.cpp
	include/farsidebackend.h
	src/farsidebackend.cpp
	include/farsideqr.h
	src/farsideqr.cpp
	include/farsideupdate.h
	src/farsideupdate.cpp
	include/discoverymanager.h
	src/discoverymanager.cpp
	include/streamsession.h
	src/streamsession.cpp
	include/sessionlog.h
	src/sessionlog.cpp
	include/settings.h
	src/settings.cpp
	include/host.h
	src/host.cpp
	res/resources.qrc
	include/controllermanager.h
	src/controllermanager.cpp
	include/psnaccountid.h
	src/psnaccountid.cpp
	include/psntoken.h
	src/psntoken.cpp
	include/jsonrequester.h
	src/jsonrequester.cpp
	src/qml/qml.qrc
	include/qmlmainwindow.h
	src/qmlmainwindow.cpp
	include/qmlbackend.h
	src/qmlbackend.cpp
	include/qmlcontroller.h
	src/qmlcontroller.cpp
	include/qmlsettings.h
	src/qmlsettings.cpp
	include/qmlsvgprovider.h
	src/qmlsvgprovider.cpp
	include/systemdinhibit.h
	src/systemdinhibit.cpp
	)
set(RESOURCE_FILES "")

if(APPLE)
	list(APPEND RESOURCE_FILES "chiaking.icns")
elseif(WIN32)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/chiaking.rc.in
		${CMAKE_CURRENT_SOURCE_DIR}/chiaking.rc
		@ONLY)
	list(APPEND RESOURCE_FILES "chiaking.rc")
endif()

if(CHIAKI_ENABLE_STEAMDECK_NATIVE AND LINUX)
	list(APPEND SOURCE_FILES
		include/sdinputcontext.h
		src/sdinputcontext.cpp
	)
endif()

if(APPLE)
	list(APPEND SOURCE_FILES
		include/macMicPermission.h
		src/macMicPermission.m
		include/macWakeSleep.h
		src/macWakeSleep.cpp
	)
elseif(WIN32)
	list(APPEND SOURCE_FILES
		include/windowsWakeSleep.h
		src/windowsWakeSleep.cpp
	)
endif()

add_executable(chiaki WIN32
		${RESOURCE_FILES}
		${SOURCE_FILES}
)
target_include_directories(chiaki PRIVATE include)

add_library(placebo-libav-impl src/libav_impl.c)
target_link_libraries(placebo-libav-impl PkgConfig::LIBPLACEBO FFMPEG::avcodec FFMPEG::avutil FFMPEG::avformat)
target_link_libraries(chiaki PkgConfig::LIBPLACEBO placebo-libav-impl)

target_link_libraries(chiaki chiaki-lib)
if(CHIAKI_ENABLE_CLI)
	add_definitions(-DCHIAKI_ENABLE_CLI)
	target_link_libraries(chiaki chiaki-cli-lib)
endif()

target_link_libraries(chiaki Qt::Core Qt::Gui Qt::GuiPrivate Qt::Concurrent Qt::Svg Qt::Qml Qt::Quick Qt::Widgets Qt::Network ZXing::ZXing)
if(WIN32)
	# Farside: the key store is encrypted with the Windows data-protection API.
	target_link_libraries(chiaki crypt32)
endif()
if (${Qt6DBus_FOUND} AND NOT WIN32)
	target_link_libraries(chiaki Qt::DBus)
if (NOT APPLE)
	target_compile_definitions(chiaki PRIVATE CHIAKI_HAVE_DBUS)
endif()
endif()
if (${Qt6WebEngineQuick_FOUND})
	target_link_libraries(chiaki Qt::WebEngineQuick)
	target_compile_definitions(chiaki PRIVATE CHIAKI_HAVE_WEBENGINE)
endif()
target_link_libraries(chiaki SDL2::SDL2)
if(CHIAKI_ENABLE_SPEEX)
  target_link_libraries(chiaki PkgConfig::SpeexDSP)
  target_compile_definitions(chiaki PRIVATE CHIAKI_GUI_ENABLE_SPEEX)
endif()
if(CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
	target_compile_definitions(chiaki PRIVATE CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
endif()
if(CHIAKI_ENABLE_SETSU)
	target_link_libraries(chiaki setsu)
	target_compile_definitions(chiaki PRIVATE CHIAKI_GUI_ENABLE_SETSU)
endif()
if(CHIAKI_ENABLE_STEAMDECK_NATIVE)
	target_link_libraries(chiaki sdeck)
	target_compile_definitions(chiaki PRIVATE QT_STATICPLUGIN CHIAKI_GUI_ENABLE_STEAMDECK_NATIVE)
endif()
if(CHIAKI_ENABLE_STEAM_SHORTCUT)
	target_link_libraries(chiaki cpp-steam-tools)
	target_compile_definitions(chiaki PRIVATE CHIAKI_GUI_ENABLE_STEAM_SHORTCUT)
endif()
if(NOT CHIAKI_ENABLE_FFMPEG_DECODER)
	message(FATAL_ERROR "Chiaki GUI requires CHIAKI_ENABLE_FFMPEG_DECODER=ON")
endif()
if(APPLE)
	target_link_libraries(chiaki "-framework AVFoundation" "-framework IOKit")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fanalyzer")
set_target_properties(chiaki PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME chiaki-ng
		MACOSX_BUNDLE_BUNDLE_VERSION ${CHIAKI_VERSION}
		MACOSX_BUNDLE_COPYRIGHT "thestr4ng3r (AGPLv3)"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.streetpea.chiaking"
		MACOSX_BUNDLE_ICON_FILE chiaking.icns
		RESOURCE "${RESOURCE_FILES}")

# Farside: the program file is Farside.exe (the target keeps its upstream name).
set_target_properties(chiaki PROPERTIES OUTPUT_NAME Farside)

# Farside: the version the updater compares with the newest GitHub release (tag v<version>), and the
# name of the Setup program it fetches from that release. Farside Host is this target, chiaki.
# (A test of the updater builds an older number: -DFARSIDE_VERSION=0.0.1.)
set(FARSIDE_VERSION 0.1.4 CACHE STRING "Version of Farside Host and Farside Client")
target_compile_definitions(chiaki PRIVATE FARSIDE_VERSION="${FARSIDE_VERSION}" FARSIDE_SETUP_NAME="FarsideHost-Setup.exe")

# Farside Client: the program for the PCs played from. It pairs with a Farside Host and shares this PC's
# controller with it. Nothing of the stream, the PS5 or the receiving side's pages is compiled into it.
find_package(OpenSSL REQUIRED)
set(FARSIDE_CLIENT_RC "")
if(WIN32)
	string(REPLACE "." "," FARSIDE_VERSION_COMMA ${FARSIDE_VERSION})
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/farsideclient.rc.in ${CMAKE_CURRENT_SOURCE_DIR}/farsideclient.rc @ONLY)
	set(FARSIDE_CLIENT_RC farsideclient.rc)
endif()
add_executable(farside-client WIN32
	src/farsideclient_main.cpp
	include/farsidelink.h
	src/farsidelink.cpp
	include/farsidesession.h
	src/farsidesession.cpp
	include/farsidebackend.h
	src/farsidebackend.cpp
	include/farsideqr.h
	src/farsideqr.cpp
	include/farsideupdate.h
	src/farsideupdate.cpp
	src/qml/client.qrc
	${FARSIDE_CLIENT_RC})
target_include_directories(farside-client PRIVATE include)
target_compile_definitions(farside-client PRIVATE FARSIDE_CLIENT_EDITION FARSIDE_VERSION="${FARSIDE_VERSION}" FARSIDE_SETUP_NAME="FarsideClient-Setup.exe")
target_link_libraries(farside-client Qt::Core Qt::Gui Qt::Qml Qt::Quick Qt::Network Qt::Svg ZXing::ZXing OpenSSL::Crypto)
if(WIN32)
	target_link_libraries(farside-client crypt32)
endif()
set_target_properties(farside-client PROPERTIES OUTPUT_NAME FarsideClient)

install(TARGETS chiaki
		RUNTIME DESTINATION bin
		BUNDLE DESTINATION bin)
install(FILES chiaking.desktop DESTINATION share/applications)
install(FILES chiaking.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES io.github.streetpea.Chiaki4deck.appdata.xml DESTINATION share/metainfo)
