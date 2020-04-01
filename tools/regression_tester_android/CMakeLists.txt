cmake_minimum_required(VERSION 3.2)
project(acl_regression_tester_android_gradle_shim NONE)

# Set our project root since our gradle files used to build live in the binary output directory
# but the actual source files live in the source/current directory.
file(RELATIVE_PATH ACL_PROJECT_ROOT ${CMAKE_CURRENT_BINARY_DIR}/app ${PROJECT_SOURCE_DIR}/app)

# Setup our state
set(ACL_ANDROID_PROJECT_NAME "ACL Regression Tests")
set(ACL_ANDROID_PROJECT_PACKAGE_NAME "com.acl.regression_tests")

# Configure gradle for our build configuration
configure_file(${ACL_ANDROID_MISC_DIR}/app/build.gradle.in app/build.gradle @ONLY)
configure_file(${ACL_ANDROID_MISC_DIR}/app/src/main/AndroidManifest.xml.in app/src/main/AndroidManifest.xml @ONLY)
configure_file(${ACL_ANDROID_MISC_DIR}/app/src/main/res/values/strings.xml.in app/src/main/res/values/strings.xml @ONLY)
configure_file(${ACL_ANDROID_MISC_DIR}/settings.gradle.in settings.gradle @ONLY)

# Copy gradle related files
file(COPY
	${ACL_ANDROID_MISC_DIR}/build.gradle
	${ACL_ANDROID_MISC_DIR}/gradle.properties
	${ACL_ANDROID_MISC_DIR}/gradle
	${ACL_ANDROID_MISC_DIR}/gradlew
	${ACL_ANDROID_MISC_DIR}/gradlew.bat
	DESTINATION .)

file(COPY
	${ACL_ANDROID_MISC_DIR}/app/proguard-rules.pro
	DESTINATION app)

# Copy our regression clip data
if(EXISTS ${TEST_DATA_DIR})
	set(TEST_DATA_CONFIG_DIR_SRC "${TEST_DATA_DIR}/../configs")
	file(GLOB_RECURSE ALL_TEST_DATA_FILES LIST_DIRECTORIES false ${TEST_DATA_DIR}/*.*)
	file(GLOB_RECURSE ALL_TEST_DATA_CONFIG_FILES LIST_DIRECTORIES false ${TEST_DATA_CONFIG_DIR_SRC}/*.*)
	file(COPY ${ALL_TEST_DATA_FILES} DESTINATION app/src/main/assets)
	file(COPY ${ALL_TEST_DATA_CONFIG_FILES} DESTINATION app/src/main/assets)
else()
	message(WARNING "Test data directory not found")
endif()

add_custom_target(${PROJECT_NAME} ALL
	COMMAND gradlew.bat
	# Decide whether we should build Debug or Release
	$<$<CONFIG:Debug>:assembleDebug>
	$<$<CONFIG:Release>:assembleRelease>)
