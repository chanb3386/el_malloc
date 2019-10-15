// Template for parsing an ELF file to print its symbol table. You are
// free to rename any variables that appear below as you see fit.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

int DEBUG = 0;                  // controls whether to print debug messages

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("usage: %s [-d] <file>\n",argv[0]);
    return 0;
  }
  char *objfile_name = argv[1];

  // check for debug mode
  if(argc >=3){
    if(strcmp("-d",argv[1])==0){ // command line arg -d enables debug printing
      DEBUG = 1;
      objfile_name = argv[2];
    }
    else{
      printf("incorrect usage\n");
      return 1;
    }
  }

  // Open file descriptor and set up memory map for objfile_name
  int fd = open(objfile_name, O_RDONLY);
  struct stat stat_buf;
  fstat(fd, &stat_buf);
  int size = stat_buf.st_size;
  char *file_bytes = mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);

  // CREATE A POINTER to the intial bytes of the file which are an ELF64_Ehdr struct
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *) file_bytes;


  // CHECK e_ident field's bytes 0 to for for the sequence {0x7f,'E','L','F'}.
  // Exit the program with code 1 if the bytes do not match
  if(ehdr->e_ident[0] != 0x7f
  || ehdr->e_ident[1] != 'E'
  || ehdr->e_ident[2] != 'L'
  || ehdr->e_ident[3] != 'F') {
    printf("Magic bytes wrong, this is not an ELF file\n");
    return 1;
  }


  // PROVIDED: check for a 64-bit file
  if(ehdr->e_ident[EI_CLASS] != ELFCLASS64){
    printf("Not a 64-bit file ELF file\n");
    return 1;
  }

  // PROVIDED: check for x86-64 architecture
  if(ehdr->e_machine != EM_X86_64){
    printf("Not an x86-64 file\n");
    return 1;
  }

  // DETERMINE THE OFFSET of the Section Header Array (e_shoff), the
  // number of sections (e_shnum), and the index of the Section Header
  // String table (e_shstrndx). These fields are from the ELF File
  // Header.
  long offset = ehdr->e_shoff;
  int num_sects = ehdr->e_shnum;
  int index = ehdr->e_shstrndx;

  // Set up a pointer to the array of section headers. Use the section
  // header string table index to find its byte position in the file
  // and set up a pointer to it.
  Elf64_Shdr *sec_hdrs = (void *) ehdr + offset;
  int bytes = sec_hdrs[index].sh_offset;
  char *sec_names =  (void *) ehdr + bytes;
  // Search the Section Header Array for the secion with name .symtab
  // (symbol table) and .strtab (string table).  Note their positions
  // in the file (sh_offset field).  Also note the size in bytes
  // (sh_size) and and the size of each entry (sh_entsize) for .symtab
  // so its number of entries can be computed.
  long sym_offset = -1;
  long str_offset = -1;
  long sym_size = -1;
  //long str_size = -1;
  long sym_ent = -1;
  //long str_ent = -1;
  int found_sym = 0;
  int found_str = 0;

  for(int i=0; i<num_sects; i++){
    if(strcmp(sec_names+(sec_hdrs[i].sh_name),".symtab") == 0) {
      sym_offset = sec_hdrs[i].sh_offset;
      sym_size = sec_hdrs[i].sh_size;
      sym_ent = sec_hdrs[i].sh_entsize;
      found_sym = 1;
    } if(strcmp(sec_names+(sec_hdrs[i].sh_name),".strtab") == 0) {
      str_offset = sec_hdrs[i].sh_offset;
      //str_size = sec_hdrs[i].sh_size;
      //str_ent = sec_hdrs[i].sh_entsize;
      found_str = 1;
    }
  }

  if(!found_sym){
    printf("Couldn't find symbol table\n");
    return 1;
  }

  if(!found_str){
    printf("Couldn't find string table\n");
    return 1;
  }

  // PRINT byte information about where the symbol table was found and
  // its sizes. The number of entries in the symbol table can be
  // determined by dividing its total size in bytes by the size of
  // each entry.
  printf("Symbol Table\n");
  printf("- %ld bytes offset from start of file\n",sym_offset);
  printf("- %ld bytes total size\n",sym_size);
  printf("- %ld bytes per entry\n",sym_ent);
  printf("- %ld entries\n", sym_size/sym_ent);


  // Set up pointers to the Symbol Table and associated String Table
  // using offsets found earlier.
  Elf64_Sym *sym_table = (void *) ehdr + sym_offset;
  char *str_table = (void *) ehdr + str_offset;


  // Print column IDs for info on each symbol
  printf("[%3s]  %8s %4s %s\n",
         "idx","TYPE","SIZE","NAME");

  long symtab_num = sym_size/sym_ent;

  int name_offset = 0;
  long size_section = 0;
  char *type;
  // Iterate over the symbol table entries
  for(int i=0; i<symtab_num; i++){

    // Determine size of symbol and name. Use <NONE> name has zero
    // length.
    size_section = sym_table[i].st_size;
    name_offset = sym_table[i].st_name;
    char *name = str_table + name_offset;
    if(name[0] == '\0') {
      name = "<NONE>";
    }

    // Determine type of symbol. See assignment specification for
    // fields, macros, and definitions related to this.
    unsigned char typec = ELF64_ST_TYPE(sym_table[i].st_info);
    if(typec == STT_NOTYPE) {
      type = "NOTYPE";
    } else if(typec == STT_OBJECT) {
      type = "OBJECT";
    } else if(typec == STT_FUNC) {
      type = "FUNC";
    } else if(typec == STT_FILE) {
      type = "FILE";
    } else if(typec == STT_SECTION) {
      type = "SECTION";
    }
    // Print symbol information
    printf("[%3d]: %8s %4lu %s\n",i,type,size_section,name);

  }

  munmap(file_bytes,size);
  close(fd);
  // Unmap file from memory and close associated file descriptor
  return 0;
}
