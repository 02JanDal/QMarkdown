cmake_minimum_required(VERSION 2.8.9)

project(QMarkdown)

set(CMAKE_AUTOMOC ON)

include(../MultiMC5/cmake/UseCXX11.cmake)

find_package(Qt5Widgets REQUIRED)

set(SRCS
	QMarkdown.h
	QMarkdown.cpp
	QMarkdownEditor.h
	QMarkdownEditor.cpp
	QMarkdownViewer.h
	QMarkdownViewer.cpp
)

add_library(QMarkdownLib STATIC ${SRCS})
qt5_use_modules(QMarkdownLib Widgets)

add_executable(QMarkdownDemo main.cpp)
qt5_use_modules(QMarkdownDemo Widgets)
target_link_libraries(QMarkdownDemo QMarkdownLib)
