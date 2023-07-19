#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGSIZE 32 // Register size
#define MEMSIZE 0x1000001 // Memory size
unsigned int Mem[MEMSIZE];
unsigned Regs[REGSIZE];

const char RegName[REGSIZE][6] = {
	"$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
	"$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
	"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
	"$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
}; // Register name

typedef struct ContorlSignals {
	int RegDst;
	int AluSrc;
	int MemToReg;
	int RegWrite;
	int MemRead;
	int MemWrite;
	int Branch;
	int Jump;
	int AluOp[2];
}Signal; // Control Signal

typedef struct instruction {
	unsigned op; //31-26
	unsigned rs; //25-21
	unsigned rt; //20-16
	unsigned rd; //15-11
	unsigned shamt; //10-6
	unsigned funct; //5-0
	unsigned offset; //15-0
	unsigned j; //25-0
	unsigned extend_value;

}Instruction; // Decode Instruction

unsigned data1, data2;
unsigned mem_data; 
unsigned AluResult;
unsigned instruction;
unsigned pc = 0; // PC count

int clockCycle = 1;
int ItypeCount = 0;
int RtypeCount = 0;
int JtypeCount = 0;
int MemoryCount = 0;

char type;

int instruction_fetch();
void pc_update();
void decode(unsigned n, Instruction* ist);
void sign_extend(Instruction* ist);
void control(Instruction* ist, Signal* signal);
void read_Register(Instruction* ist, Signal* signal);
void alu(Instruction* ist);
void memory_control(Signal* signal, Instruction* ist);
void register_control(Signal* signal, Instruction* ist);
void branch_control(Signal* signal, Instruction* ist);
void jump_control(Signal* signal, Instruction* ist);
void print_result(Instruction* ist);

int main() {

	FILE* input_file;
	int success; // Success of Instruction_fetch 
	input_file = fopen("input2.bin", "rb");

	if (input_file == NULL) {
		perror("Fail");
		exit(EXIT_FAILURE);
	}

	fread(Mem, sizeof(unsigned int), MEMSIZE, input_file);
	fclose(input_file);  // store in Memory

	for (int i = 0; i < 29; i++) {
		Regs[i] = 0;
	}


	Regs[31] = 0xFFFFFFFF;
	Regs[30] = 0;
	Regs[29] = 0x1000000;
	for (int i = 0; i < MEMSIZE; i++) {
		unsigned int num =
			((Mem[i] >> 24) & 0xff) |
			((Mem[i] << 8) & 0xff0000) |
			((Mem[i] >> 8) & 0xff00) |
			((Mem[i] << 24) & 0xff000000);
		Mem[i] = num;
	}

	Instruction ist;
	Signal signal;

	while (pc != 0xFFFFFFFF) {
		success = instruction_fetch();
		if (!success) {
			printf("That's not a correct value.");
			break;
		}
		else {
			pc_update();
		}
		decode(instruction, &ist);
		sign_extend(&ist);
		control(&ist, &signal);
		read_Register(&ist, &signal);
		alu(&ist);;
		memory_control(&signal, &ist);
		register_control(&signal, &ist);
		branch_control(&signal, &ist);
		jump_control(&signal, &ist);
		print_result(&ist);
		printf("\n");
		clockCycle++;

	}
	printf("\n\n");
	printf("	  **RESULT**	\n");
	printf("Result Value = %d   ", Regs[2]);
	printf("Total Cycle = %d\n", clockCycle - 1);
	printf("R Type Instruction = %d\n", RtypeCount);
	printf("I Type Instruction = %d\n", ItypeCount);
	printf("J type Instruction = %d\n", JtypeCount);
	printf("Memory Count = %d\n", MemoryCount);

	return 0;

}
int instruction_fetch() {
	if (pc % 4 != 0) {
		return 0;
	}
	if (pc / 4 >= MEMSIZE) {
		return 0; 
	}
	instruction = Mem[pc / 4];
	return 1;
}
void pc_update() {
	pc += 4;
}
void decode(unsigned n, Instruction* ist) {
	unsigned bit6 = 0x0000003f; //6bit total;
	unsigned bit5 = 0x0000001f; //5 bits total
	unsigned bit16 = 0x0000ffff; //16 bits total
	unsigned bit26 = 0x03ffffff; //26 bits total
	ist->op = (n >> 26) & bit6;
	ist->rs = (n >> 21) & bit5;
	ist->rt = (n >> 16) & bit5;
	ist->rd = (n >> 11) & bit5;
	ist->shamt = (n >> 6) & bit5;
	ist->funct = n & bit6;
	ist->offset = n & bit16;
	ist->j = n & bit26;

}
void sign_extend(Instruction* ist) {
	unsigned extend = 0xFFFF0000;
	unsigned Negative = ist->offset >> 15;

	if (Negative == 1)
		ist->extend_value = ist->offset | extend;
	else
		ist->extend_value = ist->offset & 0x0000ffff;
}
void control(Instruction* ist, Signal* signal) {

	if (ist->op == 0) {
		signal->AluOp[0] = 1;
		signal->AluOp[1] = 0;
	} //R-type
	else if (ist->op == 4 || ist->op == 5) {
		signal->AluOp[0] = 0;
		signal->AluOp[1] = 1;
	} //I-type branch
	else if (ist->op == 2 || ist->op == 3) {
		type = 'J';
		JtypeCount++;
		signal->RegDst = 0;
		signal->AluSrc = 0;
		signal->MemToReg = 0;
		signal->RegWrite = 0;
		signal->MemRead = 0;
		signal->MemWrite = 0;
		signal->Branch = 0;
		signal->Jump = 1;
		return;
	}
	else {
		signal->AluOp[0] = 0;
		signal->AluOp[1] = 0;
	} //I-type
	if (signal->AluOp[0] == 0 && signal->AluOp[1] == 0) {
		type = 'I';
		ItypeCount++; //I-type
		if (ist->op == 35) {
			signal->RegDst = 0;
			signal->AluSrc = 1;
			signal->MemToReg = 1;
			signal->RegWrite = 1;
			signal->MemRead = 1;
			signal->MemWrite = 0;
			signal->Branch = 0;
			signal->Jump = 0;
		} // LW
		else if (ist->op == 43) {
			signal->RegDst = 0;
			signal->AluSrc = 1;
			signal->MemRead = 0;
			signal->RegWrite = 0;
			signal->MemToReg = 0;
			signal->MemWrite = 1;
			signal->Branch = 0;
			signal->Jump = 0;
		} //SW
		else {
			signal->RegDst = 0;
			signal->AluSrc = 1;
			signal->MemToReg = 0;
			signal->RegWrite = 1;
			signal->MemRead = 0;
			signal->MemWrite = 0;
			signal->Branch = 0;
			signal->Jump = 0;
		} //addi , slti


	}
	else if (signal->AluOp[0] == 1 && signal->AluOp[1] == 0) {
		type = 'R';
		RtypeCount++;
		if (instruction == 0) {
			type = 'X';
			RtypeCount--;
		}
		signal->RegDst = 1;
		signal->AluSrc = 0;
		signal->MemToReg = 0;
		signal->RegWrite = 1;
		signal->MemRead = 0;
		signal->MemWrite = 0;
		signal->Branch = 0;
		signal->Jump = 0;

	}  //r-type
	else if (signal->AluOp[0] == 0 && signal->AluOp[1] == 1) {
		type = 'I';
		ItypeCount++;
		signal->RegDst = 0;
		signal->AluSrc = 0;
		signal->MemToReg = 0;
		signal->RegWrite = 0;
		signal->MemRead = 0;
		signal->MemWrite = 0;
		signal->Branch = 1;
		signal->Jump = 0;
	} //branch
}

void read_Register(Instruction* ist, Signal* signal) {
	data1 = Regs[ist->rs];
	if (signal->AluSrc == 1) {
		data2 = ist->extend_value;
	}
	else {
		data2 = Regs[ist->rt];
	}

}
void alu(Instruction* ist) {

	switch (ist->op) {
	case 0: //r-type
		switch (ist->funct) {
		case 32: //add
			AluResult = data1 + data2;
			break;
		case 33: //addu
			AluResult = data1 + data2;
			break;
		case 34: //sub
			AluResult = data1 - data2;
			break;
		case 35: //subu
			AluResult = data1 - data2;
			break;
		case 36: //and
			AluResult = data1 & data2;
			break;
		case 37: //or //move
			if (ist->rt == 0) {
				AluResult = data1 + 0;
			}
			else {
				AluResult = data1 | data2;
			}
			break;
		case 42: //slt
			AluResult = (data1 < data2) ? 1 : 0;
			break;
		case 0: //sll
			AluResult = data2 << ist->shamt;
			break;
		case 2: //srl
			AluResult = data2 >> ist->shamt;
			break;
		case 24: //mul
			AluResult = data1 * data2;
			break;
		case 26: //div
			AluResult = data1 / data2;
			break;
		case 8: //jr
			pc = data1;
			break;
		}
	case 2: //j
		break;
	case 3: //jal
		Regs[31] = pc;
		break;
	case 4: //beq
		AluResult = data1 - data2;
		break;
	case 5: //bne
		AluResult = data1 - data2;
		break;
	case 8: //addi
		AluResult = data1 + data2;
		break;
	case 9: //addiu
		AluResult = data1 + data2;
		break;
	case 10: //slti
		AluResult = (data1 < data2) ? 1 : 0;
		break;
	case 35: //load
		AluResult = data2 + data1;
		break;
	case 43: //store
		AluResult = data2 + data1;
		break;
	}
}
void memory_control(Signal* signal, Instruction* ist) {

	if (signal->MemWrite == 1 && signal->MemRead == 0) //sw 
	{
		Mem[AluResult / sizeof(unsigned int)] = Regs[ist->rt];
		MemoryCount++;

	}
	else if (signal->MemWrite == 0 && signal->MemRead == 1) //lw 
	{
		mem_data = Mem[AluResult / sizeof(unsigned int)];
		MemoryCount++;

	}
}
void register_control(Signal* signal, Instruction* ist) {
	if (signal->RegWrite == 1) {
		switch (ist->op) {
		case 0:
			if (ist->funct != 8) {
				Regs[ist->rd] = AluResult;
			} //jrÁ¦¿Ü
			break;
		case 8:
			Regs[ist->rt] = AluResult;
			break;
		case 9:
			if (ist->rs == 0) {
				Regs[ist->rt] = ist->extend_value;
			} //ii
			else {
				Regs[ist->rt] = AluResult;
			}
			break;
		case 10:
			Regs[ist->rt] = AluResult;
			break;
		case 35:
			Regs[ist->rt] = mem_data;
			break;
		}
	}
}
void branch_control(Signal* signal, Instruction* ist) {
	if (signal->Branch == 1 && AluResult == 0 && ist->op == 4) {
		pc = pc + (ist->extend_value << 2);
	}
	else if (signal->Branch == 1 && AluResult != 0 && ist->op == 5) {
		pc = pc + (ist->extend_value << 2);
	}
}
void jump_control(Signal* signal, Instruction* ist) {
	if (signal->Jump == 1) {
		pc = (ist->j << 2) | (pc & 0xf0000000);
	}
}
void print_result(Instruction* ist) {

	printf("ClockCycle:[%x]  ", clockCycle);
	printf("pc = %x\n", pc);
	printf("Type: %c\n", type);
	if (type == 'R') {
		printf("Opcode:%u   %s=%d   %s=%d   %s=%d\n", ist->op, RegName[ist->rs], Regs[ist->rs], RegName[ist->rt], Regs[ist->rt], RegName[ist->rd], Regs[ist->rd]);
		printf("fucntionCode=%u   shamt=%u", ist->funct, ist->shamt);
		printf("\n");
	}
	else if (type == 'I') {
		printf("Opcode:%u  %s=%d   %s=%d\n", ist->op, RegName[ist->rs], Regs[ist->rs], RegName[ist->rt], Regs[ist->rt]);
		printf("immediate=%d", ist->extend_value);
		printf("\n");

	}
	else if (type == 'J') {
		printf("Opcode:%u  ", ist->op);
		printf("address:%x", pc);
		printf("\n");
	}
	else if (instruction == 0) {
		printf("Non Operation\n");
	}

}