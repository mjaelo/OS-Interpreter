/**
	SexyOS
	FileManager.h
	Przeznaczenie: Zawiera klas� FileManager oraz deklaracje metod i konstruktor�w

	@author Tomasz Kilja�czyk
	@version 04/01/19
*/

/*
 * Aby �atwiej nawigowa� po moim kodzie polecam z�o�y� wszystko
 * Skr�t: CTRL + M + A
 */

#ifndef SEXYOS_FILEMANAGER_H
#define SEXYOS_FILEMANAGER_H

#define _CRT_SECURE_NO_WARNINGS

#include "Semaphores.hpp"
#include <string>
#include <array>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <map>

class Planista;
class proc_tree;

//Do u�ywania przy funkcji open (nazwy m�wi� same za siebie)
#define FILE_OPEN_R_MODE  1 //01
#define FILE_OPEN_W_MODE  2 //10

//Do u�ycia przy obs�udze b��d�w
#define FILE_ERROR_NONE				0
#define FILE_ERROR_EMPTY_NAME		1
#define FILE_ERROR_NAME_TOO_LONG	2
#define FILE_ERROR_NAME_USED		3
#define FILE_ERROR_NO_INODES_LEFT	4
#define FILE_ERROR_DATA_TOO_BIG		5
#define FILE_ERROR_NOT_FOUND		6
#define FILE_ERROR_NOT_OPENED		7
#define FILE_ERROR_OPENED			8
#define FILE_ERROR_SYNC				9
#define FILE_ERROR_NOT_R_MODE		10
#define FILE_ERROR_NOT_W_MODE		11

#define FILE_SYNC_WAITING 30	

//Klasa zarz�dcy przestrzeni� dyskow� i systemem plik�w
class FileManager {
private:
	//--------------------------- Aliasy ------------------------
	using u_int = unsigned int;
	using u_short_int = unsigned short int;



	//--------------- Definicje sta�ych statycznych -------------

	static const uint8_t BLOCK_SIZE = 32;	   		//Rozmiar bloku (bajty)
	static const u_short_int DISK_CAPACITY  = 1024;	//Pojemno�� dysku (bajty)
	static const uint8_t BLOCK_INDEX_NUMBER  = 3;	//Warto�� oznaczaj�ca d�ugo�� pola blockDirect
	static const uint8_t INODE_NUMBER_LIMIT	 = 32;	//Maksymalna ilo�� element�w w katalogu
	static const uint8_t MAX_FILENAME_LENGTH = 16;	//Maksymalna d�ugo�� �cie�ki

	static const bool BLOCK_FREE = false;           //Warto�� oznaczaj�ca wolny blok
	static const bool BLOCK_OCCUPIED = !BLOCK_FREE; //Warto�� oznaczaj�ca zaj�ty blok

	//Maksymalny rozmiar danych
	static const u_short_int MAX_DATA_SIZE = (BLOCK_INDEX_NUMBER + BLOCK_SIZE / 2)*BLOCK_SIZE;

	//Maksymalny rozmiar pliku (wliczony blok indeksowy)
	static const u_short_int MAX_FILE_SIZE = MAX_DATA_SIZE + BLOCK_SIZE;



	//---------------- Definicje struktur i klas ----------------

	//Klasa i-w�z�a - zawiera podstawowe informacje o pliku
	struct Inode {
		//Podstawowe informacje
		uint8_t blocksOccupied = 0;  //Ilo�� zajmowanych blok�w
		u_short_int realSize = 0;    //Rzeczywisty rozmiar pliku (rozmiar danych)
		std::array<u_int, BLOCK_INDEX_NUMBER> directBlocks{};	//Bezpo�rednie indeksy
		u_int singleIndirectBlocks; //Indeks bloku indeksowego, zpisywanego na dysku

		//Dodatkowe informacje
		tm creationTime = tm();		//Czas i data utworzenia
		tm modificationTime = tm(); //Czas i data ostatniej modyfikacji pliku

		//Synchronizacja
		Semaphore sem;
		bool opened = false;

		Inode();

		virtual ~Inode() = default;

		void clear();
	};

	struct Disk {
		//Tablica reprezentuj�ca przestrze� dyskow� (jeden indeks - jeden bajt)
		std::array<char, DISK_CAPACITY> space{};

		//----------------------- Konstruktor -----------------------
		Disk();

		//-------------------------- Metody -------------------------
		void write(const u_short_int& begin, const std::string& data);
		void write(const u_short_int& begin, const std::array<u_int, BLOCK_SIZE / 2>& data);

		const std::string read_str(const u_int& begin) const;
		const std::array<u_int, BLOCK_SIZE / 2> read_arr(const u_int& begin) const;
	} disk; //Struktura dysku
	struct FileSystem {
		u_int freeSpace{ DISK_CAPACITY }; //Zawiera informacje o ilo�ci wolnego miejsca na dysku (bajty)

		//Wektor bitowy blok�w (domy�lnie: 0 - wolny blok, 1 - zaj�ty blok)
		std::bitset<DISK_CAPACITY / BLOCK_SIZE> bitVector;

		/**
		 Tablica i-w�z��w
		 */
		std::array<Inode, INODE_NUMBER_LIMIT> inodeTable;
		//Pomocnicza tablica 'zaj�to�ci' i-w�z��w (1 - zaj�ty, 0 - wolny).
		std::bitset<INODE_NUMBER_LIMIT> inodeBitVector;
		std::unordered_map<std::string, u_int> rootDirectory;

		FileSystem();

		u_int get_free_inode_id();

		void reset();
	} fileSystem; //System plik�w

	//Klasa odczytu/zapisu
	class FileIO {
	private:
#define READ_FLAG 0
#define WRITE_FLAG 1

		std::string buffer;
		u_short_int readPos = 0;
		Disk* disk;
		Inode* file;

		bool readFlag;
		bool writeFlag;

	public:
		FileIO() : disk(nullptr), file(nullptr), readFlag(false), writeFlag(false) {}
		FileIO(Disk* disk, Inode* inode, const std::bitset<2>& mode) : disk(disk), file(inode),
			readFlag(mode[READ_FLAG]), writeFlag(mode[WRITE_FLAG]) {}

		void buffer_update(const int8_t& blockNumber);

		std::string read(const u_short_int& byteNumber);
		std::string read_all();
		void reset_read_pos() { readPos = 0; }

		void write(const std::vector<std::string>& dataFragments, const int8_t& startIndex) const;

		const std::bitset<2> get_flags() const;
	};



	//------------------- Definicje zmiennych -------------------
	bool messages = false; //Zmienna do w��czania/wy��czania powiadomie�
	bool detailedMessages = false; //Zmienna do w��czania/wy��czania szczeg�owych powiadomie�

	//Mapa dost�pu dla poszczeg�lnych plik�w i proces�w
	//Klucz   - para nazwa pliku, nazwa procesu
	//Warto�� - semafor przypisany danemu procesowi
	std::map<std::pair<std::string, std::string>, FileIO> accessedFiles;

	//Inne modu�y
	Planista* p;
	proc_tree* tree;



public:
	//----------------------- Konstruktor -----------------------
	/**
		Konstruktor domy�lny. Przypisuje do obecnego katalogu katalog g��wny.
	*/
	explicit FileManager(Planista* plan, proc_tree* tree_) : p(plan), tree(tree_) {}



	//-------------------- Podstawowe Metody --------------------
	/**
		Tworzy plik o podanej nazwie w obecnym katalogu.\n
		Po stworzeniu plik jest otwarty w trybie do zapisu.

		@param name Nazwa pliku.
		@param procName Nazwa procesu tworz�cego.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_create(const std::string& name, const std::string& procName);

	/**
		Zapisuje podane dane w danym pliku usuwaj�c poprzedni� zawarto��.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry chce zapisa� do pliku.
		@param data Dane do zapisu.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_write(const std::string& name, const std::string& procName, const std::string& data);

	/**
		Dopisuje podane dane na koniec pliku.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry chce dopisa� do pliku.
		@param data Dane do zapisu.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_append(const std::string& name, const std::string& procName, const std::string& data);

	/**
		Odczytuje podan� liczb� bajt�w z pliku. Po odczycie przesuwa si� wska�nik odczytu.\n
		Aby zresetowa� wska�nik odczytu nale�y ponownie otworzy� plik.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry chce odczyta� z pliku.
		@param byteNumber Ilo�� bajt�w do odczytu.
		@param result Miejsce do zapisania odczytanych danych.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_read(const std::string& name, const std::string& procName, const u_short_int& byteNumber, std::string& result);

	/**
		Odczytuje ca�e dane z pliku.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry chce odczyta� z pliku.
		@param result Miejsca do zapisania odczytanych danych.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_read_all(const std::string& name, const std::string& procName, std::string& result);

	/**
		Usuwa plik o podanej nazwie znajduj�cy si� w obecnym katalogu.\n
		Plik jest wymazywany z katalogu g��wnego oraz wektora bitowego.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry chce usun�� pliku.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_delete(const std::string& name, const std::string& procName);

	/**
		Otwiera plik z podanym trybem dost�pu:
		- R (read) - do odczytu
		- W (write) - do zapisu
		- RW (read/write) - do odczytu i zapisu

		@param name Nazwa pliku.
		@param procName Nazwa procesu tworz�cego.
		@param mode Tryb dost�pu do pliku.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_open(const std::string& name, const std::string& procName, const unsigned int& mode);

	/**
		Zamyka plik o podanej nazwie.

		@param name Nazwa pliku.
		@param procName Nazwa procesu, kt�ry zamkn�� pliku.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_close(const std::string& name, const std::string& procName);



	//--------------------- Dodatkowe metody --------------------

	/**
		Tworzy plik o podanej nazwie w obecnym katalogu i zapisuje w nim podane dane.
		Po stworzeniu plik jest otwarty w trybie do zapisu.

		@param name Nazwa pliku.
		@param procName Nazwa procesu tworz�cego.
		@param data Dane typu string.
		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_create(const std::string& name, const std::string& procName, const std::string& data);

	/**
		Zamyka wszystkie pliki dla danego procesu.

		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_close_all(const std::string& procName);

	/**
		Zamyka wszystkie pliki.

		@return Kod b��du. 0 oznacza brak b��du.
	*/
	int file_close_all();

	/**
		Zmienia zmienn� odpowiadaj�c� za wy�wietlanie komunikat�w.
		false - komunikaty wy��czone.
		true - komunikaty w��czone.

		@param onOff Czy komunikaty maj� by� w��czone.
		@return void.
	*/
	void set_messages(const bool& onOff);

	/**
		Zmienia zmienn� odpowiadaj�c� za wy�wietlanie szczeg�owych komunikat�w.
		false - komunikaty wy��czone.
		true - komunikaty w��czone.

		@param onOff Czy komunikaty maj� by� w��czone.
		@return void.
	*/
	void set_detailed_messages(const bool& onOff);



	//------------------ Metody do wy�wietlania -----------------

	/**
		Wy�wietla parametry systemu plik�w.

		@return void.
	*/
	static void display_file_system_params();

	/**
		Wy�wietla informacje o wybranym katalogu.

		@return void.
	*/
	void display_root_directory_info();

	/**
		Wy�wietla informacje o pliku.

		@return True, je�li operacja si� uda�a lub false, je�li operacja nie powiod�a si�.
	*/
	int display_file_info(const std::string& name);

	/**
		Wy�wietla struktur� katalog�w.

		@return True, je�li operacja si� uda�a lub false, je�li operacja nie powiod�a si�.
	*/
	void display_root_directory();

	/**
		Wy�wietla zawarto�� dysku jako znaki.
		'.' - puste pole.

		@return void.
	*/
	void display_disk_content_char();

	/**
		Wy�wietla wektor bitowy.

		@return void.
	*/
	void display_bit_vector();



	//------ KOLEJNE METODY MA�O KOGO POWINNY OBCHODZI� ---------
private:
	//------------------- Metody Sprawdzaj�ce -------------------

	bool check_if_name_used(const std::string& name);

	bool check_if_enough_space(const u_int& dataSize) const;



	//-------------------- Metody Obliczaj�ce -------------------

	static u_int calculate_needed_blocks(const size_t& dataSize);

	size_t calculate_directory_size();

	size_t calculate_directory_size_on_disk();



	//--------------------- Metody Alokacji ---------------------

	void file_truncate(Inode* file, const u_int& neededBlocks);

	void file_add_indexes(Inode* file, const std::vector<u_int>& blocks);

	void file_deallocate(Inode* file);

	void file_allocate_blocks(Inode* file, const std::vector<u_int>& blocks);

	void file_allocation_increase(Inode* file, const u_int& neededBlocks);

	const std::vector<u_int> find_unallocated_blocks_fragmented(u_int blockNumber);

	const std::vector<u_int> find_unallocated_blocks_best_fit(const u_int& blockNumber);

	const std::vector<u_int> find_unallocated_blocks(const u_int& blockNumber);



	//----------------------- Metody Inne -----------------------

	bool is_file_opened_write(const std::string& fileName);

	int file_accessing_proc_count(const std::string& fileName);

	std::string get_file_data_block(Inode* file, const int8_t& indexNumber) const;

	void file_write(Inode* file, FileIO* IO, const std::string& data);

	void file_append(Inode* file, FileIO* IO, const std::string& data);

	static const tm get_current_time_and_date();

	void change_bit_vector_value(const u_int& block, const bool& value);

	static const std::vector<std::string> fragmentate_data(const std::string& data);
};

#endif //SEXYOS_FILEMANAGER_H
