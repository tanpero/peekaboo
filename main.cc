#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "./json.hpp"

using json = nlohmann::json;

// 用于解析 .sln 文件内容，包括项目名称、依赖关系、配置信息、生成文件类型等
json parseSolution(const std::string& slnContent) {
    json solution;
    std::istringstream ss(slnContent);
    std::string line;

    // 解析项目名称
    if (std::getline(ss, line)) {
        size_t start = line.find('"') + 1;
        size_t end = line.rfind('"');
        std::string projectName = line.substr(start, end - start);
        solution["projectName"] = projectName;
    }

    // 解析依赖关系
    std::vector<std::string> dependencies;
    while (std::getline(ss, line)) {
        if (line.find("Project") != std::string::npos) {
            size_t start = line.find('"') + 1;
            size_t end = line.rfind('"');
            std::string projectName = line.substr(start, end - start);
            dependencies.push_back(projectName);
        }
    }
    solution["dependencies"] = dependencies;

    // 解析配置信息
    std::map<std::string, std::string> configurations;
    while (std::getline(ss, line)) {
        if (line.find("GlobalSection(ProjectConfigurationPlatforms)") != std::string::npos) {
            break;
        }
    }
    while (std::getline(ss, line)) {
        if (line.find("EndGlobalSection") != std::string::npos) {
            break;
        }
        size_t delimiter = line.find('.');
        size_t start = line.find('"', delimiter) + 1;
        size_t end = line.rfind('"');
        std::string projectName = line.substr(start, end - start);
        std::string configuration = line.substr(0, delimiter);
        configurations[projectName] = configuration;
    }
    solution["configurations"] = configurations;

    // 解析生成文件类型
    std::map<std::string, std::string> fileTypes;
    while (std::getline(ss, line)) {
        if (line.find("ProjectSection(ProjectDependencies)") != std::string::npos) {
            break;
        }
    }
    while (std::getline(ss, line)) {
        if (line.find("EndProjectSection") != std::string::npos) {
            break;
        }
        size_t start = line.find('=') + 1;
        size_t end = line.find(']', start);
        std::string projectName = line.substr(start, end - start);
        start = line.find('[', end) + 1;
        end = line.find(']', start);
        std::string fileType = line.substr(start, end - start);
        fileTypes[projectName] = fileType;
    }
    solution["fileTypes"] = fileTypes;

    return solution;
}

// 用于解析 .vcxproj 文件内容，将不同作用的字段进行分类
json parseCppProject(const std::string& vcxprojContent) {
    json project;
    std::istringstream ss(vcxprojContent);
    std::string line;

    // 解析配置信息
    std::map<std::string, std::string> configurations;
    while (std::getline(ss, line)) {
        if (line.find("<ItemDefinitionGroup") != std::string::npos) {
            break;
        }
    }
    while (std::getline(ss, line)) {
        if (line.find("</ItemDefinitionGroup") != std::string::npos) {
            break;
        }
        size_t start = line.find("Condition=\"'") + 12;
        size_t end = line.find("'", start);
        std::string configuration = line.substr(start, end - start);
        start = line.find("<Configuration>") + 15;
        end = line.find("</Configuration>", start);
        std::string configurationValue = line.substr(start, end - start);
        configurations[configuration] = configurationValue;
    }
    project["configurations"] = configurations;

    // 解析编译选项
    std::map<std::string, std::string> compileOptions;
    while (std::getline(ss, line)) {
        if (line.find("<ClCompile") != std::string::npos) {
            break;
        }
    }
    while (std::getline(ss, line)) {
        if (line.find("</ClCompile") != std::string::npos) {
            break;
        }
        size_t start = line.find("Include=\"") + 9;
        size_t end = line.find("\"", start);
        std::string sourceFile = line.substr(start, end - start);
        std::string compileOption;
        while (std::getline(ss, line)) {
            if (line.find("</ClCompile") != std::string::npos) {
                break;
            }
            if (line.find("<AdditionalOptions>") != std::string::npos) {
                start = line.find("<AdditionalOptions>") + 19;
                end = line.find("</AdditionalOptions>", start);
                compileOption = line.substr(start, end - start);
                break;
            }
        }
        compileOptions[sourceFile] = compileOption;
    }
    project["compileOptions"] = compileOptions;

    return project;
}

nlohmann::json buildFramework(nlohmann::json& solution) {
  // 遍历 solution 中的每个子项目
  for (auto& project : solution["projects"]) {
    std::string projectPath = project["path"];
    
    // 读取子项目的内容
    std::ifstream file(projectPath);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // 调用 parseCppProject 解析子项目
    nlohmann::json parsedProject = parseCppProject(content);
    
    // 将解析的子项目信息整合到 solution 的对应子项目中
    project["parsedProject"] = parsedProject;
  }
  
  // 返回整合完成的 solution 框架
  return solution;
}


std::string generateCmakeLists(nlohmann::json& solution) {
  std::string cmakeLists;
  
  // 遍历 solution 中的每个子项目
  for (auto& project : solution["projects"]) {
    std::string projectName = project["name"];
    std::string projectType = project["parsedProject"]["type"];
    
    // 根据子项目信息生成对应的 add_executable 或 add_library 语句
    if (projectType == "Executable") {
      cmakeLists += "add_executable(" + projectName + " " + project["parsedProject"]["sourceFiles"] + ")\n";
    } else if (projectType == "Library") {
      cmakeLists += "add_library(" + projectName + " " + project["parsedProject"]["sourceFiles"] + ")\n";
    }
  }
  
  // 返回生成的 cmakeLists 字符串
  return cmakeLists;
}


int main() {
    // 示例使用
    std::string slnContent = R"(Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio 15
VisualStudioVersion = 15.0.28010.2003
MinimumVisualStudioVersion = 10.0.40219.1
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "Project1", "Project1\Project1.vcxproj", "{4F91E518-696F-40B9-BB75-1FFBFF0444F6}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "Project2", "Project2\Project2.vcxproj", "{56975423-DE68-4F56-9D04-B57B6AB8F3C5}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Win32 = Debug|Win32
		Release|Win32 = Release|Win32
		EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{4F91E518-696F-40B9-BB75-1FFBFF0444F6}.Debug|Win32.ActiveCfg = Debug|Win32
		{4F91E518-696F-40B9-BB75-1FFBFF0444F6}.Debug|Win32.Build.0 = Debug|Win32
		{4F91E518-696F-40B9-BB75-1FFBFF0444F6}.Release|Win32.ActiveCfg = Release|Win32
		{4F91E518-696F-40B9-BB75-1FFBFF0444F6}.Release|Win32.Build.0 = Release|Win32
		{56975423-DE68-4F56-9D04-B57B6AB8F3C5}.Debug|Win32.ActiveCfg = Debug|Win32
		{56975423-DE68-4F56-9D04-B57B6AB8F3C5}.Debug|Win32.Build.0 = Debug|Win32
		{56975423-DE68-4F56-9D04-B57B6AB8F3C5}.Release|Win32.ActiveCfg = Release|Win32
		{56975423-DE68-4F56-9D04-B57B6AB8F3C5}.Release|Win32.Build.0 = Release|Win32
		EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal)";

    std::string vcxprojContent = R"(<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <ClCompile Include="source1.cpp">
      <AdditionalOptions>/std:c++20</AdditionalOptions>
    </ClCompile>
    <ClCompile Include="source2.cpp">
      <AdditionalOptions>/std:c++17</AdditionalOptions>
    </ClCompile>
  </ItemGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">
    <Configuration>Debug</Configuration>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">
    <Configuration>Release</Configuration>
  </PropertyGroup>
</Project>)";

    std::cout << generateCmake()

    return 0;
}
