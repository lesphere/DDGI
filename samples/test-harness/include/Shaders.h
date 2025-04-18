/*
* Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Common.h"
#include "Configs.h"

#include <dxcapi.h>

#include "shaderc/shaderc.hpp"

namespace Shaders
{
    struct ShaderCompiler
    {
    #if _WIN32
        HINSTANCE             dll = nullptr;
    #elif __linux__
        void*                 dll = nullptr;
    #endif
        IDxcUtils*            utils = nullptr;
        IDxcCompiler3*        compiler = nullptr;
        IDxcIncludeHandler*   includes = nullptr;

        DxcCreateInstanceProc DxcCreateInstance = nullptr;
        Configs::Shaders      config = {};

        std::string           root = "";
        std::string           rtxgi = "";
    };

    struct ShaderProgram
    {
        std::wstring               filepath = L"";
        std::wstring               targetProfile = L"lib_6_6";
        std::wstring               entryPoint = L"";
        std::wstring               exportName = L"";
        std::vector<std::wstring>  includePath;
        std::vector<LPCWSTR>       arguments;
        std::vector<std::wstring*> defineStrs;
        std::vector<DxcDefine>     defines;

        IDxcBlob*                  bytecode = nullptr;
        IDxcBlobWide*              shaderName = nullptr;

        shaderc_shader_kind        kind;
        std::vector<uint32_t>      spirv;

        void Release()
        {
            for (size_t defineIndex = 0; defineIndex < defineStrs.size(); defineIndex++)
            {
                SAFE_DELETE(defineStrs[defineIndex]);
            }
            defineStrs.clear();
            defines.clear();
            arguments.clear();
            includePath.clear();
            spirv.clear();
            SAFE_RELEASE(bytecode);
            SAFE_RELEASE(shaderName);
        }
    };

    struct ShaderPipeline
    {
        ShaderProgram vs;
        ShaderProgram ps;
        uint32_t numStages() const { return 2; };

        void Release()
        {
            vs.Release();
            ps.Release();
        }
    };

    struct ShaderRTHitGroup
    {
        ShaderProgram chs;
        ShaderProgram ahs;
        ShaderProgram is;
        LPCWSTR exportName = L"";

        bool hasCHS() const { return (chs.bytecode != nullptr) || !chs.spirv.empty(); }
        bool hasAHS() const { return (ahs.bytecode != nullptr) || !ahs.spirv.empty(); }
        bool hasIS() const { return (is.bytecode != nullptr) || !is.spirv.empty(); }
        uint32_t numStages() const { return (hasCHS() + hasAHS() + hasIS()); }
        uint32_t numSubobjects() const { return (1 + numStages()); }

        void Release()
        {
            chs.Release();
            ahs.Release();
            is.Release();
        }
    };

    struct ShaderRTPipeline
    {
        uint32_t payloadSizeInBytes = 0;

        ShaderProgram rgs;
        ShaderProgram miss;
        std::vector<ShaderRTHitGroup> hitGroups;

        void Release()
        {
            rgs.Release();
            miss.Release();
            for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(hitGroups.size()); hitGroupIndex++)
            {
                hitGroups[hitGroupIndex].Release();
            }
            hitGroups.clear();
        }
    };

    bool Initialize(const Configs::Config& config, ShaderCompiler& compiler);
    void AddDefine(ShaderProgram& shader, std::wstring name, std::wstring value);
    bool Compile(ShaderCompiler& compiler, ShaderProgram& shader, bool warningsAsErrors = true);
    bool CompileGLSL(ShaderProgram& shader, bool warningsAsErrors = true);
    void Cleanup(ShaderCompiler& compiler);
}
