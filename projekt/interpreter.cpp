#include "interpreter.h"
#include "FileManager.h"
#include "MemoryManager.h"
#include "Procesy.h"
#include "pipe.h"
#include <iostream>





void display_file_error_text(const int &outcome) { //komunikaty o bledach (w sumie tylko do plikow)
	if (outcome == FILE_ERROR_NONE) { return; }
	else if (outcome == FILE_ERROR_EMPTY_NAME) { std::cout << "Pusta nazwa!\n"; }
	else if (outcome == FILE_ERROR_NAME_TOO_LONG) { std::cout << "Nazwa za dluga!\n"; }
	else if (outcome == FILE_ERROR_NAME_USED) { std::cout << "Nazwa zajeta!\n"; }
	else if (outcome == FILE_ERROR_NO_INODES_LEFT) { std::cout << "Osiagnieto limit plikow!\n"; }
	else if (outcome == FILE_ERROR_DATA_TOO_BIG) { std::cout << "Dane za duze!\n"; }
	else if (outcome == FILE_ERROR_NOT_FOUND) { std::cout << "Nie znaleziono pliku!\n"; }
	else if (outcome == FILE_ERROR_NOT_OPENED) { std::cout << "Plik nie jest otwarty!\n"; }
	else if (outcome == FILE_ERROR_NOT_R_MODE) { std::cout << "Plik nie jest do odczytu!\n"; }
	else if (outcome == FILE_ERROR_NOT_W_MODE) { std::cout << "Plik nie jest do zapisu!\n"; }
	else { std::cout << "Nie obsluzony blad: " << outcome << "\n"; }
}

Interpreter::Interpreter(FileManager* fileManager_, MemoryManager* memoryManager_, proc_tree* tree_,Pipeline* pipeline_) : fileManager(fileManager_),
memoryManager(memoryManager_), tree(tree_), pipeline(pipeline_), A(0), B(0), C(0), D(0) {}

std::array<std::string, 3> Interpreter::instruction_separate(const std::string& instructionWhole)
{
	std::string temp;
	std::array<std::string, 3> wynik;// mnemonik, rejestr, rejestr
	unsigned int pozycjaWyniku = 0;

	for (const char& c : instructionWhole)
	{
		if (c != ' ')
		{
			temp += c;
		}
		else if (!temp.empty())
		{
			wynik[pozycjaWyniku] = temp;
			temp.clear();
			pozycjaWyniku++;
		}
	}
	return wynik;
}

void Interpreter::jump_pos_set(const std::string& procName) {
	if (instrBeginMap.find(address) != instrBeginMap.end()) {
		instruction_counter = address - 1;
		RAM_pos = instrBeginMap[address];//skok do adr jesli adr juz zmapowany
	}
	else
	{
		//Szukanie najdajszej zmapowanej pozycji
		while (true)
		{//idzie do nast rozkazu, i jesli go nie zna to mapuje w 2gim while
			if (instrBeginMap.find(instruction_counter) == instrBeginMap.end()) { break; }
			//jesli nie znaleziono danego rozkazu w mapie to przerywa i idzie do 2giego while'a by go zmapowac
			else
			{//jesli rozkaz zmapowany to idzie do natepnego
				RAM_pos = instrBeginMap[instruction_counter];
				// jesli znaleziono adres w mapie to sprawdza kolejna pozycje
				instruction_counter++;
			}
		}
		while (instruction_counter < address) {//mapowanie nowych rozkazow, bez ich wykonywania az dojdzie do naszego adresu
			std::string temp = memoryManager->get(tree->find_proc(procName), RAM_pos);
			temp += ' ';
			RAM_pos += temp.length();
			instrBeginMap[instruction_counter] = RAM_pos;
			instruction_counter++;
		} // gdy dojdzie to rampos jest na odpowiedniej pozycji i dopiero wtedy bedzie wykonywac rozkaz
		instruction_counter--; //W wykonaj program si� podnosi
	}
}

void Interpreter::registers_state()
{
	//std::cout << "Rozkaz: " << instructionWhole;
	A = tree->proc.A;
	B = tree->proc.B;
	C = tree->proc.C;
	D = tree->proc.D;
	instruction_counter = tree->proc.comand_counter;
}

void Interpreter::stan_rejestrow() const
{
	std::cout << "\nA: " << A << "\nB: " << B << "\nC: " << C << "\nD: " << D << "\nilosc rozkazow: " << instruction_counter << "\n\n";
}

void Interpreter::execute_program(const std::string& procName) {
	instruction_counter = 0;
	RAM_pos = 0;
	instrBeginMap.clear();
	instrBeginMap[0] = 0; //Pierwszy rozkaz zawsze tak samo

	while (true)
	{
		std::string instructionWhole;

		//Je�li jeszcze nie wpisane w map�
		if (instrBeginMap.find(instruction_counter + 1) == instrBeginMap.end()) {
			instructionWhole = memoryManager->get(tree->find_proc(procName), RAM_pos);
			instructionWhole += ' '; //Spacja na ko�cu u�atwia rozdzielanie
			RAM_pos += instructionWhole.length();
			instrBeginMap[instruction_counter + 1] = RAM_pos;
		}
		else {
			RAM_pos = instrBeginMap[instruction_counter];
			instructionWhole = memoryManager->get(tree->find_proc(procName), RAM_pos);//pobieranie linijki kodu
			instructionWhole += ' '; //Spacja na ko�cu u�atwia rozdzielanie
			RAM_pos += instructionWhole.length();
		}

		std::cout << "Rozkaz (IC " << instruction_counter << "): " << instructionWhole << '\n';

		if (!execute_instruction(instructionWhole, procName)) { break; }// jesli napotkano na hlt, to konczymy program
		instruction_counter++;
	}
}


bool Interpreter::execute_instruction(const std::string& instructionWhole, const std::string& procName)
{
	PCB* runningProc = tree->find_proc(procName);
	std::array<std::string, 3> instructionParts = instruction_separate(instructionWhole);
	const std::string instruction = instructionParts[0];

	//registers_state();
	number = -1; address = -1;
	std::string nazwa1 = ""; //string w ""
	std::string nazwa2 = ""; //string w ""

	int *reg1 = &A;
	int *reg2 = reg1;

	//Wpisywanie warto�ci do rej1 (pierwszy wyraz rozkazu)
	if (!instructionParts[1].empty()) {
			 if (instructionParts[1] == "A") reg1 = &A;
		else if (instructionParts[1] == "B") reg1 = &B;
		else if (instructionParts[1] == "C") reg1 = &C;
		else if (instructionParts[1] == "D") reg1 = &D;
		else if (instructionParts[1][0] == '[')
		{
			instructionParts[1].erase(instructionParts[1].begin());
			instructionParts[1].pop_back();
			address = std::stoi(instructionParts[1]);
		}
		else if (instructionParts[1][0] == '"')
		{
			instructionParts[1].erase(instructionParts[1].begin());
			instructionParts[1].pop_back();
			nazwa1 = instructionParts[1];
		}
		else { number = std::stoi(instructionParts[2]); reg1 = &number; }
	}

	//Wpisywanie warto�ci do rej2 (drugi wyraz rozkazu)
	if (!instructionParts[2].empty()) {
			 if (instructionParts[2] == "A") reg2 = &A;
		else if (instructionParts[2] == "B") reg2 = &B;
		else if (instructionParts[2] == "C") reg2 = &C;
		else if (instructionParts[2] == "D") reg2 = &D;
		else if (instructionParts[2] == "R") *reg2 = FILE_OPEN_R_MODE;
		else if (instructionParts[2] == "W") *reg2 = FILE_OPEN_W_MODE;
		else if (instructionParts[2][0] == '[')
		{
			instructionParts[2].erase(instructionParts[2].begin());
			instructionParts[2].pop_back();
			address = std::stoi(instructionParts[2]);
		}
		else if (instructionParts[2][0] == '"')
		{
			instructionParts[2].erase(instructionParts[2].begin());
			instructionParts[2].pop_back();
			nazwa2 = instructionParts[2];
		}
		else { number = std::stoi(instructionParts[2]); reg2 = &number; }
	}

	//Rozkazy interpretacja
	{
		if (instruction == "ADD")
		{
			*reg1 += *reg2;
		}
		else if (instruction == "SUB")
		{
			if (*reg1 - *reg2 > 0)
				*reg1 -= *reg2;
			else
				std::cout << "wynik mniejszy od 0\n";
		}
		else if (instruction == "MUL")
		{
			*reg1 *= *reg2;
		}
		else if (instruction == "DIV")
		{
			if (*reg2 == 0)
				std::cout << "nie wykonano rozkazu, dzielenie przez 0\n";
			else *reg1 /= *reg2;
		}
		else if (instruction == "MOV")
		{
			*reg1 = *reg2;
		}
		else if (instruction == "INC")
		{
			(*reg1)++;
		}
		else if (instruction == "DEC")
		{
			(*reg1)--;
		}


		//Rozkazy skoki
		else if (instruction == "JMP")
		{
			jump_pos_set(procName);
		}
		else if (instruction == "JZ") //jump if zero , skok warunkowy
		{
			if (*reg1 == 0)
			{
				jump_pos_set(procName);
			}
		}


		//Rozkazy pliki
		else if (instruction == "MF")// stworzenie pliku
		{
			display_file_error_text(fileManager->file_create(nazwa1,runningProc->name));
		}
		else if (instruction == "OF") // otwarcie pliku, ma flage ze jest otwarty
		{
			if (fileManager->file_open(nazwa1, runningProc->name, *reg2) != FILE_ERROR_NONE) {
				std::cout << "Blad!\n";
			}
		}
		else if (instruction == "WF")//nadpisz do pliku
		{
			display_file_error_text(fileManager->file_write(nazwa1,runningProc->name, std::to_string(*reg2)));
		}
		else if (instruction == "AF")//dopisz do pliku
		{
			display_file_error_text(fileManager->file_append(nazwa1,runningProc->name, std::to_string(*reg2)));
		}
		else if (instruction == "CF")//ZAMKNIJ plik
		{
			display_file_error_text(fileManager->file_close(nazwa1,runningProc->name));
		}
		else if (instruction == "RF")//CZYTANIE Z PLIKU
		{
			std::string temp;
			display_file_error_text(fileManager->file_read_all(nazwa1,runningProc->name, temp));
			std::cout << "\na prog to " << temp;

		}


		//Rozkazy procesy
		else if (instruction == "CP") //tworzenie procesu
		{
			tree->fork(new PCB(nazwa1,runningProc->PID),runningProc->proces_size);
		}
		else if (instruction == "DP") //zabijanie procesu
		{
			tree->exit(tree->find_proc(nazwa1)->PID);
		}


		//Rozkazy potoki
		else if (instruction == "SP") //stworz potok
		{
		pipeline->createPipe(runningProc->name,nazwa1 );//rodzic,dziecko  
		}
		else if (instruction == "UP") //usun potok
		{
		pipeline->deletePipe(runningProc->name, nazwa1);
		}
		else if (instruction == "SM") //send message 
		{
			
				if (nazwa2.size() > 0) pipeline->write(runningProc->name, nazwa1, nazwa2);
				else
					pipeline->write(runningProc->name, nazwa1, std::to_string(*reg2));
			
		}
		else if (instruction == "RM") //read message 
		{
		if(pipeline->existPipe(runningProc->name,nazwa1))
		std::cout<<"odczytana wiadomosc: "<<pipeline->read( nazwa1, runningProc->name, *reg2)<<'\n';//wysylajacy, odbierajacy, dlugosc plku
		}





		//Rozkaz koniec procesu
		else if (instruction == "HLT") { return false; }

		//B��d
		else { std::cout << "error\n"; }
	}


	//teraz zapisz rejestry
	
	runningProc->A = A;
	runningProc->B = B;
	runningProc->C = C;
	runningProc->D = D;
	runningProc->comand_counter = instruction_counter;

	std::string krok;
	while (true) {
		std::cout << "\nPodaj rozkaz pracy krokowej: "; //Praca krokowa temp pozniej usune
		std::cin >> krok;
		if (krok == "r") stan_rejestrow();
		else if (krok == "dd") fileManager->display_disk_content_char(); //Displau disk
		else if (krok == "dbv") fileManager->display_bit_vector();		 //Displau bit vector
		else if (krok == "drd") fileManager->display_root_directory();   //Displau root directory
		else if (krok == "st") tree->display_tree();   //Displau root directory
		else { break; }
	}

	return true;
}
