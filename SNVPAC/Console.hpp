#pragma once

enum m_eConsoleColors : unsigned short
{
	CLR_BGREEN = 0xA,
	CLR_BRED = 0xC,
	CLR_BYELLOW = 0xE,
	CLR_BWHITE = 0xF,
	CLR_WBLACK = 0xF0,
};

namespace Console
{
	HANDLE m_Handle = nullptr;
	HWND m_Window;
	CONSOLE_SCREEN_BUFFER_INFO m_ConScreen;
	unsigned int m_DefaultColor;

	void Initialize(const char* m_Title)
	{
		m_Handle = GetStdHandle(STD_OUTPUT_HANDLE);
		m_Window = GetConsoleWindow();
		GetConsoleScreenBufferInfo(m_Handle, &m_ConScreen);
		m_DefaultColor = m_ConScreen.wAttributes;

		SetConsoleTitleA(m_Title);
	}

	void Clear()
	{
		GetConsoleScreenBufferInfo(m_Handle, &m_ConScreen);

		int m_Length = m_ConScreen.dwSize.X * m_ConScreen.dwSize.Y;
		COORD m_Coords = { 0, 0 };
		DWORD m_Dummy = 0x0;

		FillConsoleOutputCharacterA(m_Handle, ' ', m_Length, m_Coords, &m_Dummy);
		FillConsoleOutputAttribute(m_Handle, m_ConScreen.wAttributes, m_Length, m_Coords, &m_Dummy);
		SetConsoleCursorPosition(m_Handle, m_Coords);
	}

	void SetColor(unsigned short m_Color)
	{
		SetConsoleTextAttribute(m_Handle, static_cast<WORD>(m_Color));
	}

	void Print(unsigned int m_Color, char m_Bracket, std::string m_Text)
	{
		SetColor(CLR_BWHITE);
		std::cout << "[ " << m_Bracket << " ] ";

		SetColor(m_Color);
		std::cout << m_Text;

		SetColor(m_DefaultColor);
	}

	void Print(unsigned int m_Color, std::string m_Text)
	{
		SetColor(m_Color);
		std::cout << m_Text;

		SetColor(m_DefaultColor);
	}

	void DrawListbox(int m_Index, std::string* m_List, int m_Size, int m_DeltaShow, std::string m_Prefix)
	{
		int m_Start = m_Index - m_DeltaShow;
		int m_End = m_Index + m_DeltaShow;

		if (0 > m_Start)
		{
			m_End += -m_Start;
			m_Start = 0;
		}
		else if (m_End >= m_Size)
		{
			m_Start -= m_End - m_Size + 1;
			m_End = m_Size - 1;
		}
		m_Start = max(0, min(m_Start, m_Size - 1));
		m_End = max(0, min(m_End, m_Size - 1));

		static const char* m_ScrollInfo[3] = { "\x16\n", "\x1E\n", "\x1F\n" };

		Print(CLR_BWHITE, m_Prefix + m_ScrollInfo[((m_Start - 1) >= 0) ? 1 : 0]);
		for (int i = m_Start; m_End >= i; ++i)
		{
			Print(CLR_BWHITE, m_Prefix + " ");
			Print(i == m_Index ? CLR_WBLACK : CLR_BWHITE, m_List[i] + "\n");
		}
		Print(CLR_BWHITE, m_Prefix + m_ScrollInfo[((m_End + 1) < m_Size) ? 2 : 0]);
	}
}