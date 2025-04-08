/*
* Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Shaders.h"
#include "graphics/UI.h"

#include "spirv-tools/libspirv.hpp"

#if __linux__
#include <filesystem>
#endif

#include <sstream>
#include <codecvt>
#include <filesystem>

namespace Shaders
{
    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    void UnloadDirectXCompiler(ShaderCompiler& dxc)
    {
    #if _WIN32
        FreeLibrary(dxc.dll);
    #elif __linux__
        ::dlclose(dxc.dll);
    #endif
        dxc.DxcCreateInstance = nullptr;
        dxc.dll = nullptr;
    }

    HRESULT LoadDirectXCompiler(ShaderCompiler& dxc)
    {
        if(dxc.dll != nullptr) return S_OK;

    #if _WIN32
        dxc.dll = LoadLibrary("dxcompiler.dll");
    #elif __linux__
        const std::filesystem::path subdir("bin/vulkan/libdxcompiler.so");
        std::filesystem::path path = std::filesystem::current_path();
        path = path.parent_path();
        path /= subdir;
        dxc.dll = dlopen(path.c_str(), RTLD_LAZY);
    #endif

        // DLL was not loaded, error out
        if (dxc.dll == nullptr) return HRESULT_FROM_WIN32(GetLastError());

        // Load the function
    #if _WIN32
        dxc.DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc.dll, "DxcCreateInstance");
    #elif __linux__
        dxc.DxcCreateInstance = (DxcCreateInstanceProc)::dlsym(dxc.dll, "DxcCreateInstance");
    #endif

        // DLL function not loaded
        if(dxc.DxcCreateInstance == nullptr)
        {
            UnloadDirectXCompiler(dxc);
            return HRESULT_FROM_WIN32(GetLastError());
        }

        return S_OK;
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Initialize the the DirectX Shader Compiler (DXC).
     */
    bool Initialize(const Configs::Config& config, ShaderCompiler& dxc)
    {
        // Load the DXC DLL
        if(FAILED(LoadDirectXCompiler(dxc))) return false;

        // Create the utils instance
        if(FAILED(dxc.DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc.utils)))) return false;

        // Create the compiler instance
        if(FAILED(dxc.DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc.compiler)))) return false;

        // Create the default include handler
        if(FAILED(dxc.utils->CreateDefaultIncludeHandler(&dxc.includes))) return false;

        dxc.config = config.shaders;
        dxc.root = config.app.root;
        dxc.rtxgi = config.app.rtxgi;

        return true;
    }

    /**
     * Add a define to the shader program with the given name and value.
     */
    void AddDefine(ShaderProgram& shader, std::wstring name, std::wstring value)
    {
        DxcDefine define;

        shader.defineStrs.push_back(new std::wstring(name));
        define.Name = shader.defineStrs.back()->c_str();

        shader.defineStrs.push_back(new std::wstring(value));
        define.Value = shader.defineStrs.back()->c_str();

        shader.defines.push_back(define);
    }

    // ����������wstring ת std::string��UTF-8��
    std::string WStringToString(const std::wstring& wstr)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        return conv.to_bytes(wstr);
    }

    // ������������ȡ�ļ����ݵ� std::string
    std::string LoadFileAsString(const std::wstring& filepath)
    {
        std::ifstream file(WStringToString(filepath));
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    std::string LoadFileAsString(const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    // �Զ��� Includer ʵ��
    class DefaultIncluder : public shaderc::CompileOptions::IncluderInterface {
    public:
        explicit DefaultIncluder(const std::vector<std::string>& include_dirs)
            : include_dirs_(include_dirs) {
        }

        shaderc_include_result* GetInclude(const char* requested_source,
            shaderc_include_type type,
            const char* requesting_source,
            size_t include_depth) override {
            // �������� include Ŀ¼�������ļ�
            for (const auto& dir : include_dirs_) {
                std::string full_path = dir + "/" + requested_source;
                std::string file_content = LoadFileAsString(full_path);

                if (!file_content.empty()) {
                    // ���� include ���
                    auto* result = new shaderc_include_result();
                    result->source_name = strdup(full_path.c_str());
                    result->source_name_length = full_path.size();
                    result->content = strdup(file_content.c_str());
                    result->content_length = file_content.size();
                    result->user_data = nullptr;
                    return result;
                }
            }

            std::cerr << "Include file not found: " << requested_source << std::endl;
            return nullptr;
        }

        void ReleaseInclude(shaderc_include_result* data) override {
            free(const_cast<char*>(data->source_name));
            free(const_cast<char*>(data->content));
            delete data;
        }

    private:
        std::vector<std::string> include_dirs_;
    };

    // �� SPIR-V ���������ݷ����Ϊ�ı�
    void DisassembleSpirvAndWriteToFile(const std::wstring& filepath, const std::vector<uint32_t>& spirvBinary) {
        std::filesystem::path pathObj(filepath);

        // ����չ���滻Ϊ .spv
        pathObj.replace_extension(L".spvasm");

        // ָ��ʹ�� Vulkan 1.2 �������ɸ�������ѡ����������
        spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_2);
        std::string disassembly;
        // ʹ���Ѻ�����ѡ��
        if (!spirvTools.Disassemble(spirvBinary, &disassembly,
            SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_COMMENT | SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT | SPV_BINARY_TO_TEXT_OPTION_REORDER_BLOCKS)) {
            std::cerr << "SPIR-V �����ʧ�ܣ�" << std::endl;
            throw std::runtime_error("SPIR-V �����ʧ�ܣ�");
        }

        // ����������ı�д���ļ�
        std::ofstream outFile(pathObj.wstring());
        if (!outFile) {
            std::cerr << "�޷�������ļ���" << std::endl;
            throw std::runtime_error("�޷�������ļ���");
        }
        outFile << disassembly;
        outFile.close();
    }

    // �� SPIR-V �ı����Ϊ������
    void ReadAndAssembleSpirv(const std::wstring& filepath, std::vector<uint32_t>& spirvBinary) {
        std::filesystem::path pathObj(filepath);
        std::string spirvText;

        if (pathObj.filename() == "ProbeTraceRGS.glsl") {
            pathObj.replace_extension(L".spvasm");
            spirvText = LoadFileAsString(pathObj);
            spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_2);
            if (!spirvTools.Assemble(spirvText, &spirvBinary)) {
                std::cerr << "SPIR-V ���ʧ�ܣ�" << std::endl;
                throw std::runtime_error("SPIR-V ���ʧ�ܣ�");
            }
        }
    }

    /**
     * Compile a shader with the DirectX Shader Compiler (DXC).
     */
    bool Compile(ShaderCompiler& dxc, ShaderProgram& shader, bool warningsAsErrors)
    {
        uint32_t codePage = 0;
        IDxcBlobEncoding* pShaderSource = nullptr;
        IDxcResult* result = nullptr;

        bool retry = true;
        while(retry)
        {
            // Load and encode the shader file
            if (FAILED(dxc.utils->LoadFile(shader.filepath.c_str(), nullptr, &pShaderSource))) return false;

            DxcBuffer source;
            source.Ptr = pShaderSource->GetBufferPointer();
            source.Size = pShaderSource->GetBufferSize();
            source.Encoding = DXC_CP_ACP;

            // Add default shader defines
            AddDefine(shader, L"HLSL", L"1");

            // Treat warnings as errors
            if(warningsAsErrors || dxc.config.warningsAsErrors) shader.arguments.push_back(L"-WX");

            // Disable compilation optimizations
            if(dxc.config.disableOptimizations) shader.arguments.push_back(L"-Od");

            // Disable validation
            if(dxc.config.disableValidation) shader.arguments.push_back(L"-Vd");

            // Add with debug information to compiled shaders
            if(dxc.config.shaderSymbols)
            {
                shader.arguments.push_back(L"-Zi");                      // enable debug information (symbols)
                shader.arguments.push_back(L"-Qembed_debug");            // embed shader pdb (symbols) in the shader
                if(dxc.config.lifetimeMarkers) shader.arguments.push_back(L"-enable-lifetime-markers"); // enable variable lifetime markers
            }

            // Add include directories
            std::wstring arg;
            if(!shader.includePath.empty())
            {
                for (const auto& include_dir: shader.includePath) {
                    arg.append(L"-I ");
                    arg.append(include_dir);
                    shader.arguments.push_back(arg.c_str());
                }
            }

            // Build the arguments array
            IDxcCompilerArgs* args = nullptr;
            dxc.utils->BuildArguments(
                shader.filepath.c_str(),
                shader.entryPoint.c_str(),
                shader.targetProfile.c_str(),
                shader.arguments.data(),
                static_cast<UINT>(shader.arguments.size()),
                shader.defines.data(),
                static_cast<UINT>(shader.defines.size()),
                &args);

            // Compile the shader
            if (FAILED(dxc.compiler->Compile(&source, args->GetArguments(), args->GetCount(), dxc.includes, IID_PPV_ARGS(&result)))) return false;

            // Get the errors (if there are any)
            IDxcBlobUtf8* errors = nullptr;
            if(FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr))) return false;

            // Display errors and allow recompilation
            if(errors != nullptr && errors->GetStringLength() != 0)
            {
                // Convert error blob to a std::string
                std::vector<char> log(errors->GetStringLength() + 1);
                memcpy(log.data(), errors->GetStringPointer(), errors->GetStringLength());

                std::string errorMsg = "Shader Compiler Error:\n";
                errorMsg.append(log.data());

                // Spawn a pop-up that displays the compilation errors and retry dialog
                if (Graphics::UI::MessageRetryBox(errorMsg.c_str()))
                {
                    continue; // Try to compile again
                }

                return false;
            }

            // Shader compiled successfully
            retry = false;

            // Get the shader bytecode
            if(FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader.bytecode), &shader.shaderName))) return false;
        }

        //uint32_t* spirv = reinterpret_cast<uint32_t*>(shader.bytecode->GetBufferPointer());
        //size_t size = shader.bytecode->GetBufferSize() / sizeof(spirv[0]);
        //DisassembleSpirvAndWriteToFile(shader.filepath, std::vector<uint32_t>(spirv, spirv + size));

        return true;
    }

    /**
     * Compile a shader with shaderc.
     */
    bool CompileGLSL(ShaderProgram& shader, bool warningsAsErrors) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

        // set HLSL offset for GLSL, refer to https://docs.vulkan.org/guide/latest/shader_memory_layout.html
        options.SetHlslOffsets(true);

        // ���Ĭ�Ϻ궨�弰�û��Զ����
        options.AddMacroDefinition("GLSL");
        options.AddMacroDefinition("__spirv__");
        options.AddMacroDefinition("GLSL_BACKEND_VULKAN"); // for Framework.glsl in viwo
        for (size_t i = 0; i + 1 < shader.defineStrs.size(); i += 2) {
            std::string name = WStringToString(*shader.defineStrs[i]);
            std::string value = WStringToString(*shader.defineStrs[i + 1]);
            options.AddMacroDefinition(name, value);
        }

        // ��Ӱ���Ŀ¼������У�
        if (!shader.includePath.empty()) {
            std::vector<std::string> include_dirs;
            for (const auto& wide_str : shader.includePath) {
                include_dirs.push_back(WStringToString(wide_str));
            }
            options.SetIncluder(std::make_unique<DefaultIncluder>(include_dirs));
        }

        // ���ý�������Ϊ����
        if (warningsAsErrors) {
            options.SetWarningsAsErrors();
        }

        // ������Ҫ�����Ż���shaderc Ĭ���Ż��ȼ�Ϊ zero ʱ���Ż���
        options.SetOptimizationLevel(shaderc_optimization_level_zero);

        // ָ����ڵ�
        assert(!shader.entryPoint.empty());
        std::string entryPoint = WStringToString(shader.entryPoint);

        bool retry = true;
        while (retry) {
            std::string shaderSource = LoadFileAsString(shader.filepath);
            if (shaderSource.empty()) {
                std::cerr << "�޷�������ɫ���ļ�: " << WStringToString(shader.filepath) << std::endl;
                return false;
            }

            shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
                shaderSource, shader.kind,
                WStringToString(shader.filepath).c_str(),
                entryPoint.c_str(),
                options);

            if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
                std::string errorMsg = module.GetErrorMessage();
                std::cerr << "shaderc �������:\n" << errorMsg << std::endl;
                // ��������Ϣд���ļ�
                std::ofstream logFile("shadererrorlog.txt");
                if (logFile.is_open()) {
                    logFile << "shaderc �������:\n" << errorMsg << "\n";
                    logFile.close();
                }
                else {
                    std::cerr << "�޷��� shadererror.log ����д�룡" << std::endl;
                }
                if (Graphics::UI::MessageRetryBox(errorMsg.c_str())) {
                    continue; // �û�ѡ�����ԣ������±���
                }
                return false;
            }

            // ������������ shader.spirv
            shader.spirv.assign(module.cbegin(), module.cend());
            retry = false;
        }

        DisassembleSpirvAndWriteToFile(shader.filepath, shader.spirv);

        //ReadAndAssembleSpirv(shader.filepath, shader.spirv);

        return true;
    }

    /**
     * Release memory used by the shader compiler.
     */
    void Cleanup(ShaderCompiler& dxc)
    {
        SAFE_RELEASE(dxc.utils);
        SAFE_RELEASE(dxc.compiler);
        SAFE_RELEASE(dxc.includes);
        UnloadDirectXCompiler(dxc);
        dxc.root = "";
        dxc.rtxgi = "";
    }

}
