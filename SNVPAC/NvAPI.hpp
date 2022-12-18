#pragma once
#include <Windows.h>

#define NVAPI_MODULE	"nvapi.dll"

// struct
struct NvDVCInfo_t
{
	unsigned int m_Version;
	int m_CurLevel;
	int m_MinLevel;
	int m_MaxLevel;
};

// API
class CNvAPI
{
public:
	HMODULE m_Module = 0;

	struct Functions_t
	{
		void* EnumDisplayHandle = nullptr;
		void* GetDVCInfo = nullptr;
		void* SetDVCLevel = nullptr;

		void Initalize(HMODULE m_Module)
		{
			typedef void*(*t_QueryInterface)(unsigned int);
			t_QueryInterface QueryInterface(reinterpret_cast<t_QueryInterface>(GetProcAddress(m_Module, "nvapi_QueryInterface")));

			EnumDisplayHandle = QueryInterface(0x9ABDD40D);
			GetDVCInfo = QueryInterface(0x4085DE45);
			SetDVCLevel = QueryInterface(0x172409B4);
		}
	};
	Functions_t Func;

	CNvAPI()
	{
		m_Module = LoadLibraryA(NVAPI_MODULE);
		if (!m_Module) return;

		Func.Initalize(m_Module);
	}

	~CNvAPI()
	{
		if (m_Module)
			FreeLibrary(m_Module);
	}

	bool Loaded() { return m_Module; }

	int EnumDisplayHandle(int m_Enum, int* m_Handle)
	{ return reinterpret_cast<int(*)(int, int*)>(Func.EnumDisplayHandle)(m_Enum, m_Handle); }

	int GetDVCInfo(int m_Handle, int m_OutputID, NvDVCInfo_t* m_Info)
	{ 
		static uint32_t m_Size(sizeof(NvDVCInfo_t) | 0x10000);
		memcpy(m_Info, &m_Size, sizeof(m_Size));

		return reinterpret_cast<int(*)(int, int, NvDVCInfo_t*)>(Func.GetDVCInfo)(m_Handle, m_OutputID, m_Info); 
	}

	int SetDVCLevel(int m_Handle, int m_OutputID, int m_Level)
	{ return reinterpret_cast<int(*)(int, int, int)>(Func.SetDVCLevel)(m_Handle, m_OutputID, m_Level); }
};