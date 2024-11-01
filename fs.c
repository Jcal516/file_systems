#include "fs.h"
#include "disk.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct superblock {
  uint16_t used_bitmap_offset;
  uint16_t inode_bitmap_offset;
  uint16_t inode_offset;
  uint16_t directory_block_offset;
};

struct inode {
  //ref count
  //data block array 
  //16 byte to show 13
  //point to indirect 2048 blocks 
  //uint16_t type; //1 = full
  uint16_t direct_offset;
  uint16_t single_indirect_offset;
  int size;
};

struct dir_entry {
  bool is_used;
  uint16_t inode_number;
  char name[15];
};

struct file_desc {
  bool is_used;
  uint16_t inode_number;
  int offset;
};

uint8_t used_bitmap[(1<<13)/8];
uint8_t inode_bitmap[(1<<13)/8];
struct superblock super;
struct inode inode_table[512];
struct file_desc fd_table[32];
struct dir_entry directory_block[64];
char working_buf[4096];

void set_byte_used(int bit_index) {
  int byte_index = bit_index / 8;
  int bit_offset = bit_index % 8;
  used_bitmap[byte_index] |= (1 << bit_offset);
}

void set_byte_inode(int bit_index) {
  int byte_index = bit_index / 8;
  int bit_offset = bit_index % 8;
  inode_bitmap[byte_index] |= (1 << bit_offset);
}

int get_byte_used(int bit_index) {
  int byte_index = bit_index / 8;
  int bit_offset = bit_index % 8;
  uint8_t mask = (1 << bit_offset);
  if((used_bitmap[byte_index] & mask) != 0)
    return 1;
  else
    return 0;
}

int get_byte_inode(int bit_index) {
  int byte_index = bit_index / 8;
  int bit_offset = bit_index % 8;
  uint8_t mask = (1 << bit_offset);
  if((inode_bitmap[byte_index] & mask) != 0)
    return 1;
  else
    return 0;
}

void clear_byte_used(int bit_index) {
  int byteIndex = bit_index / 8;
  int bitOffset = bit_index % 8;
  uint8_t mask = ~(1 << bitOffset);
  used_bitmap[byteIndex] &= mask;
}

void clear_byte_inode(int bit_index) {
  int byteIndex = bit_index / 8;
  int bitOffset = bit_index % 8;
  uint8_t mask = ~(1 << bitOffset);
  inode_bitmap[byteIndex] &= mask;
}

int make_fs(const char *disk_name) {
  //blocks: super, used_bitmap, inode_bitmap, directory_block, inode table
  if(make_disk(disk_name) < 0)
    return -1;
  open_disk(disk_name);
  super.used_bitmap_offset=1;
  super.inode_offset = 4;
  super.directory_block_offset=3;
  super.inode_bitmap_offset = 2;
  memcpy(&working_buf, &super,sizeof(super));
  if(block_write(0,&working_buf) < 0)
    return -1;
  if(block_write(super.inode_bitmap_offset,&inode_bitmap) < 0)
    return -1;
  for(int p = 0;p <25;p++)
    set_byte_used(p);
  if(block_write(super.used_bitmap_offset,&used_bitmap) < 0)
    return -1;
  memcpy(&working_buf, &directory_block, sizeof(directory_block));
  if(block_write(super.directory_block_offset, &working_buf) < 0)
    return -1;
  if(block_write(super.inode_offset,&(inode_table[0]))  < 0)
    return -1;
  close_disk();
  return 0;
}

int mount_fs(const char *disk_name) {
  if(open_disk(disk_name) < 0)
    return -1;
  if(block_read(0,&super) < 0)
      return -1;
  if(block_read(super.used_bitmap_offset,&used_bitmap) < 0)
    return -1;
  if(block_read(super.inode_bitmap_offset,&inode_bitmap) < 0)
    return -1;
  if(block_read(super.directory_block_offset, &directory_block) < 0)
    return -1;
  if(block_read(super.inode_offset,&(inode_table[0]))  < 0)
    return -1;
  return 0;
  }

int umount_fs(const char *disk_name) {
  for(int i=0;i<32;i++)
    fd_table[i].is_used = 0;
  for(int j=0;j<64;j++)
    directory_block[j].is_used = 0;;
  if(close_disk() < 0)
    return -1;
  return 0;
}

int fs_open(const char *name) {
  if(directory_block == NULL)
    return -1;
  for(int i = 0;i < 64;i++){
    if(directory_block[i].is_used && strcmp(directory_block[i].name,name) == 0){
      for(int j = 0;j<32;j++) {
	if(!fd_table[j].is_used) {
	  fd_table[j].is_used = 1;
	  fd_table[j].inode_number = directory_block[i].inode_number;
	  fd_table[j].offset = 0;
	  return j;
	  break;
	}
      }
      break;
    }
  }
  return -1;
}
 
int fs_close(int fildes) {
  if(fd_table[fildes].is_used){
    fd_table[fildes].is_used = 0;
    fd_table[fildes].inode_number = 0;
    fd_table[fildes].offset = 0;
    return 0;
  }
 return -1;
}

int fs_create(const char *name) {
  for(int j = 0;j < 64;j++) {
    if(directory_block[j].is_used)
      if(strcmp(directory_block[j].name,name) == 0)
	return -1;
  }
  for(int i = 0;i < 64;i++){
    if(!directory_block[i].is_used) {
      //mark in directory
      directory_block[i].is_used = 1;
      //directory_block[i].name = malloc(strlen(name) + 1);
      strcpy(directory_block[i].name,name);
      for(int j = 0;j < 512;j++) {
	//look for a free inode
	if(get_byte_inode(j)==0) {
	  set_byte_inode(j);
	  for(int seek = 25;seek <8192;seek++) {
	    if(get_byte_used(seek)==0) {
	      inode_table[j].direct_offset=seek;
	      set_byte_used(seek);
	      break;
	    }
	  }
	  block_write(super.used_bitmap_offset,used_bitmap);
	  //TODO: how to do indirect offset
	  directory_block[i].inode_number=j;
	  block_write(super.inode_offset,&(inode_table[0]));
	  break;
	  //TODO bitmap
	}
      }
      break;
    }
  } 
  block_write(super.directory_block_offset,&directory_block);
  return 0;
}

int fs_delete(const char *name) {
  for(int i = 0; i < 64; i++) {
    if(strcmp(directory_block[i].name,"") != 0  && strcmp(directory_block[i].name,name)==0) {
      for(int j = 0;j<32;j++) {
	if(fd_table[j].inode_number == directory_block[i].inode_number) {
	  if(fd_table[j].is_used == 1)
	    return -1;
	}
      }
      int in = directory_block[i].inode_number;
      uint16_t ind_arr[2048];
      if(inode_table[in].single_indirect_offset != 0)
	block_read(inode_table[in].single_indirect_offset,ind_arr);
      //clear all the indirects
      for(int b = 0;b <2048;b++)
	if(ind_arr[b] > 0){
	  clear_byte_used(ind_arr[b]);
	  ind_arr[b]=0;
	}
      block_write(inode_table[in].single_indirect_offset,ind_arr);
      //clear the indirect
      clear_byte_used(inode_table[in].single_indirect_offset);
      //clear the direct
      clear_byte_used(inode_table[in].direct_offset);
	  //clrea the inode bit
      clear_byte_inode(in);
      inode_table[in].direct_offset = 0;
      inode_table[in].single_indirect_offset = 0;
      inode_table[in].size = 0;
      //set all values to 0
      block_write(super.used_bitmap_offset,&used_bitmap);
      block_write(super.inode_bitmap_offset,&inode_bitmap);
      block_write(super.inode_offset,inode_table);
      directory_block[i].is_used = 0;
      for (int ff = 0; ff < 14; ff++) {
	directory_block[i].name[ff] = '\0';
      }
      //directory_block[i].name = "";
      //free(directory_block[i].name);
      memcpy(&working_buf,&directory_block,sizeof(directory_block));
      block_write(super.directory_block_offset,&working_buf);
      return 0;
      //TODO: unset used_bitmap
    }
  }
  return -1;
}
//How to account for offset + large files?
int fs_read(int fildes, void *buf, size_t nbyte) {
  uint16_t ind_arr[2048];
  if(fd_table[fildes].is_used) {
    uint16_t start_bn,last_bn;
     //find starting block
    if(fd_table[fildes].offset < 4096) {
      start_bn = inode_table[fd_table[fildes].inode_number].direct_offset;
    }
    else {
      block_read(inode_table[fd_table[fildes].inode_number].single_indirect_offset,ind_arr);
      start_bn = ind_arr[(fd_table[fildes].offset/4096)-1];
    }
    //find last byte
    int last_byte;
    if(inode_table[fd_table[fildes].inode_number].size < nbyte + fd_table[fildes].offset)
      last_byte = inode_table[fd_table[fildes].inode_number].size - fd_table[fildes].offset;
    else     
      last_byte = nbyte;
    //find last block
    if(fd_table[fildes].offset + last_byte <= 4096) {
      last_bn = inode_table[fd_table[fildes].inode_number].direct_offset;
    }
    else {
      block_read(inode_table[fd_table[fildes].inode_number].single_indirect_offset,ind_arr);
      last_bn = ind_arr[((fd_table[fildes].offset+last_byte-1)/4096)-1];
    }
    //read for one block
    if(start_bn == last_bn) {
      block_read(start_bn,working_buf);
      memcpy(buf,working_buf+(fd_table[fildes].offset % 4096),last_byte);
      fd_table[fildes].offset += last_byte;
      return last_byte;
    }
    else{
      int buf_offset = 0;
      uint16_t curr_bn = start_bn;
      while(curr_bn != last_bn) {
	//read blocks
	block_read(curr_bn,working_buf);
	if(curr_bn==start_bn) {
	  memcpy((char*)buf+buf_offset,working_buf+(fd_table[fildes].offset%4096),4096-(fd_table[fildes].offset%4096));
	  buf_offset += 4096-(fd_table[fildes].offset%4096);
	  fd_table[fildes].offset = fd_table[fildes].offset + (4096-(fd_table[fildes].offset%4096));
	  //next block
	  block_read(inode_table[fd_table[fildes].inode_number].single_indirect_offset,ind_arr);
	  curr_bn=ind_arr[0];
	}
	else {
	  memcpy((char*)buf+buf_offset,(working_buf),4096);
	  buf_offset += 4096;
	  fd_table[fildes].offset += 4096;
	  //next block
	  block_read(inode_table[fd_table[fildes].inode_number].single_indirect_offset,ind_arr);
	  curr_bn = ind_arr[(fd_table[fildes].offset/4096)-1];
	}
      }
      assert(curr_bn == last_bn);
      //last_bn
      block_read(curr_bn,working_buf);
      if((fd_table[fildes].offset+last_byte)%4096 == 0) {
	memcpy((char*)buf+buf_offset,(working_buf),4096);
	fd_table[fildes].offset += (4096);
      }
      else {
	memcpy((char*)buf+buf_offset,(working_buf),(fd_table[fildes].offset+last_byte)%4096);
	fd_table[fildes].offset += (fd_table[fildes].offset+last_byte)%4096;
      }
      return last_byte;
    }
  }
  return -1;
}
int fs_write(int fildes, void *buf, size_t nbyte) {
  if(fd_table[fildes].is_used) {
    uint16_t start_bn;
    if(fd_table[fildes].offset < 4096) {
      start_bn = inode_table[fd_table[fildes].inode_number].direct_offset;
    }
    else {
      uint16_t indirect_array[2048];
      block_read(inode_table[fd_table[fildes].inode_number].single_indirect_offset,&(indirect_array));
      start_bn = indirect_array[(fd_table[fildes].offset/4096)-1];
    }
  //int last_byte;
  int final_byte = fd_table[fildes].offset + nbyte;
  //Write more?
  //same block
  if((fd_table[fildes].offset%4096) + nbyte <= 4096) {
    block_read(start_bn,working_buf);
    memcpy(working_buf+(fd_table[fildes].offset%4096),buf,nbyte);
    block_write(start_bn,working_buf);
    //update file size if needed
    if(inode_table[fd_table[fildes].inode_number].size < final_byte) {
      inode_table[fd_table[fildes].inode_number].size = final_byte;
      block_write(super.inode_offset,&inode_table[0]);
    }
    //update file offset
    fd_table[fildes].offset = final_byte;
    return nbyte;
  }
  //multiple blocks
  uint16_t indirect_array[2048];
  for(int x = 0;x <2048;x++)
    indirect_array[x] =0;
  //first block (update offset)
  int curr_bn = start_bn;
  int buf_offset= 0;
  block_read(start_bn,working_buf);
  memcpy(working_buf+(fd_table[fildes].offset%4096),buf,4096 - (fd_table[fildes].offset%4096));
  buf_offset += 4096 - (fd_table[fildes].offset%4096);
  fd_table[fildes].offset = fd_table[fildes].offset + 4096-(fd_table[fildes].offset%4096);
  block_write(start_bn,working_buf);
  //other blocks
  while(fd_table[fildes].offset != final_byte) {
    //need to allocate more data?
    if(fd_table[fildes].offset/4096 > (inode_table[fd_table[fildes].inode_number].size-1) / 4096) {
      //allocate
      if( curr_bn == inode_table[fd_table[fildes].inode_number].direct_offset){ //is direct data block
	//make a new data block
	int seek;
	for(int o = 0; o <8192;o++) {
	  if(get_byte_used(o) == 0) {
	    seek = o;
	    set_byte_used(o);
	    break;
	  }
	}
	inode_table[fd_table[fildes].inode_number].single_indirect_offset = seek;
	//set indirect offset to that datablock
	//make an array of 2048 uint16_t and fill data block with that array
	//uint16_t indirect_array[2048];
	block_write(seek,&indirect_array);
	//make a new data block
	int point;
	for(int g = 25; g <8192;g++) {
	  if(get_byte_used(g) == 0) {
	    point = g;
	    set_byte_used(g);
	    break;
	  }
	}
	//make indirect offset[0] point to that data block
	indirect_array[0] = point;
	block_write(seek,&indirect_array);
      }
      else {
	//make a new data block
	int point;
	for(int g = 25; g <8192;g++) {
	  if(get_byte_used(g) == 0) {
	    point = g;
	    set_byte_used(g);
	    break;
	  }
	}
	//find indirect offset index
	for(int k = 0; k <2048;k++) {
	  if(indirect_array[k] == curr_bn) {
	    indirect_array[k+1] = point;
	    break;
	  }
	}
	block_write(inode_table[fd_table[fildes].inode_number].single_indirect_offset,&indirect_array);
	//set indirext offset index + 1 to point to new data block
      }
    }
  //jump to next block
    if( curr_bn == inode_table[fd_table[fildes].inode_number].direct_offset){ //is direct data block
      curr_bn = indirect_array[0];
    }
    else {
      for(int k = 0; k <2048;k++) {
	if(indirect_array[k] == curr_bn) {
	  curr_bn = indirect_array[k+1];
	  break;
	}
      }
    }
  //copy block to working buf
    block_read(curr_bn, working_buf);
  //whole block or last block?
    if(final_byte - fd_table[fildes].offset >=  4096) {
      memcpy(working_buf,(char*)buf+buf_offset,4096);
      fd_table[fildes].offset += 4096;
      buf_offset += 4096;
      block_write(curr_bn,working_buf);
    }
    else { //last block
      memcpy(working_buf,(char*)buf+buf_offset,(final_byte%4096));
      fd_table[fildes].offset = final_byte;
      block_write(curr_bn,working_buf);
    }
  }
  if(fd_table[fildes].offset > inode_table[fd_table[fildes].inode_number].size)
    inode_table[fd_table[fildes].inode_number].size = final_byte;
  block_write(super.inode_offset,&inode_table);
  block_write(super.used_bitmap_offset,&used_bitmap);
  return nbyte;
  }
  return -1;
}

int fs_get_filesize(int fildes) {
  if(fd_table[fildes].is_used) {
    return inode_table[fd_table[fildes].inode_number].size;
  }
  return -1;
}

int fs_listfiles(char ***files) {
  block_read(super.directory_block_offset,&directory_block);
  int c = 0;
  *files = (char**)malloc(64 * sizeof(char*));
  for(int i = 0; i < 64;i++) {
    if(directory_block[i].is_used) {
      //pointer??
      //files[c] = (char**)malloc(sizeof(char));
      (*files)[c] = (char*)malloc(sizeof(char));
      strcpy((*files)[c],directory_block[i].name);
      c++;
    }
  }
  (*files)[65] = NULL;
  return 0;
}
int fs_lseek(int fildes, off_t offset) {
  if(!fd_table[fildes].is_used)
    return-1;
  if(offset < 0)
    return -1;
  if(offset > inode_table[fd_table[fildes].inode_number].size)
    return -1;
  fd_table[fildes].offset = offset;
  return 0;

}
//How to truncate?
int fs_truncate(int fildes, off_t length) {
  if(!fd_table[fildes].is_used) {
    return -1;
  }
  if(inode_table[fd_table[fildes].inode_number].size < length)
    return -1;
  inode_table[fd_table[fildes].inode_number].size = length;
  return 0;
}

