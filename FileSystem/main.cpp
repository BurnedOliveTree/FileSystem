#include <iostream>
#include <fstream>

#define BLOCK_SIZE 1024         // half of block size
#define FILE_NAME_SIZE 256
#define ROOT_DIR "root"
#define SYSTEM_SEPARATOR '/'

typedef struct MapByte {
    unsigned a:1;
    unsigned b:1;
    unsigned c:1;
    unsigned d:1;
    unsigned e:1;
    unsigned f:1;
    unsigned g:1;
    unsigned h:1;
    
    unsigned long operator [](int i) const {
        switch (i) {
            case 0:
                return a;
            case 1:
                return b;
            case 2:
                return c;
            case 3:
                return d;
            case 4:
                return e;
            case 5:
                return f;
            case 6:
                return g;
            case 7:
                return h;
            default:
                throw "Index out of range";
        }
    }
    
    void assign(unsigned i, unsigned new_value) {
        switch (i) {
            case 0:
                a = new_value;
                break;
            case 1:
                b = new_value;
                break;
            case 2:
                c = new_value;
                break;
            case 3:
                d = new_value;
                break;
            case 4:
                e = new_value;
                break;
            case 5:
                f = new_value;
                break;
            case 6:
                g = new_value;
                break;
            case 7:
                h = new_value;
                break;
            default:
                throw "Index out of range";
        }
    }
} MapByte;

struct INode {
    unsigned long file_ptr;
    unsigned long file_size;
    unsigned long upper_folder;
    char file_type;             // 'd' for directory, 'f' for a normal file, 's' for a shortcut
    char name[FILE_NAME_SIZE];
};

struct Block {
    char data[BLOCK_SIZE];
    unsigned long files_amount = 0;
    unsigned long files[BLOCK_SIZE * sizeof(char) / sizeof(unsigned long) - 1];
};

class FileSystem {
    unsigned long inode_amount;
    unsigned long block_amount;
    MapByte* inode_map;
    MapByte* block_map;
    INode* inodes;
    Block* blocks;
    unsigned long curr_dir;
    
    unsigned long alloc_blocks(unsigned long size) {
        unsigned long temp_size = 0;
        unsigned long result = 0;
        for (int i=0; i<inode_amount/8; i++) {
            for (int j=0; j<8; j++) {
                if (block_map[i][j] == 0) {
                    if (temp_size == 0)
                        result = i * 8 + j;
                    temp_size++;
                    if (temp_size == size) {
                        for (int k=0; k<size; k++)
                            block_map[(result+k)/8].assign((result+k)%8, 1);
                        return result;
                    }
                }
                else
                    temp_size = 0;
            }
        }
        throw "Unable to allocate that amount of memory";
    }
    
    unsigned long alloc_node() {
        for (int i=0; i<inode_amount/8; i++) {
            for (int j=0; j<8; j++) {
                if (inode_map[i][j] == 0) {
                    inode_map[i].assign(j, 1);
                    return i * 8 + j;
                }
            }
        }
        throw "Unable to find a free inode";
    }
    
    void free_blocks(unsigned long first_block, unsigned long size) {
        for (unsigned long i=first_block; i<first_block+size; i++) {
            block_map[i / 8].assign(i % 8, 0);
        }
    }
    
    void free_node(unsigned long inode) {
        inode_map[inode / 8].assign(inode % 8, 0);
    }
    
    Block* get_block_by_inode(unsigned long inode) {
        return &blocks[inodes[inode].file_ptr];
    }
    
    unsigned long get_inode_by_name(std::string name) {
        return get_inode_by_name(name, curr_dir);
    }
    
    unsigned long get_inode_by_name(std::string name, unsigned long dir) {
        if (name.find(SYSTEM_SEPARATOR) == std::string::npos) {
            Block* curr_block = get_block_by_inode(dir);
            if (name == "..") {
                return inodes[dir].upper_folder;
            }
            else {
                for (int i=0; i<curr_block->files_amount; i++) {
                    if (inodes[curr_block->files[i]].name == name) {
                        return curr_block->files[i];
                    }
                }
                throw "Requested node not found";
            }
        }
        else {
            unsigned long first_sep = name.find(SYSTEM_SEPARATOR);
            return get_inode_by_name(name.substr(first_sep+1, name.size()-first_sep-1), get_inode_by_name(name.substr(0, first_sep), dir));
        }
    }
    
    void do_delete(unsigned long inode) {
        unsigned long dir = inodes[inode].upper_folder;                                 // get inode of upper dir
        inodes[dir].file_size -= inodes[inode].file_size;                               // reduce size of upper dir
        while (dir != 0) {                                                              // reduce size of all upper dirs
            dir = inodes[dir].upper_folder;                                             // move to upper dir
            inodes[dir].file_size -= inodes[inode].file_size;                           // reduce size of upper dir
        }
        Block* upper_block = get_block_by_inode(inode);
        unsigned long dir_pos = BLOCK_SIZE;
        for (int i=0; i<upper_block->files_amount; i++) {                               // get position of this file in the upper fir 'files' array
            if (inodes[upper_block->files[i]].name == inodes[inode].name) {
                dir_pos = i;
                break;
            }
        }
        for (unsigned long i=dir_pos+1; i<upper_block->files_amount; i++) {             // move all next dir/files one place earlier in 'files' array of upper dir
            upper_block->files[i-1] = upper_block->files[i];
        }
        get_block_by_inode(inodes[inode].upper_folder)->files_amount--;                 // decrement files_amount of upper dir
        if (inodes[inode].file_type != 's')
            free_blocks(inodes[inode].file_ptr, inodes[inode].file_size);               // free all of this file blocks
        free_node(inodes[inode].file_ptr);                                              // free inode of this file
    }
    
    void do_delete_recursive(unsigned long inode) {
        // free inodes and blocks of subdirs/subfiles
        unsigned long files_amount = blocks[inodes[inode].file_ptr].files_amount;
        for (int i=0; i<files_amount; i++) {
            if (inodes[blocks[inodes[inode].file_ptr].files[i]].file_type == 'd')
                do_delete_recursive(blocks[inodes[inode].file_ptr].files[i]);
            else
                do_delete(blocks[inodes[inode].file_ptr].files[i]);
        }
        do_delete(inode);
    }
    
    void set_block(unsigned long b, unsigned long n, unsigned long upper_folder, unsigned long file_size, char file_type, std::string name) {
        inodes[n].upper_folder = upper_folder;
        inodes[n].file_ptr = b;
        inodes[n].file_size = file_size;
        inodes[n].file_type = file_type;
        strcpy(inodes[n].name, name.c_str());
    }
    
    void file_info(unsigned long inode) {
        std::cout << "File \"" << inodes[inode].name << "\"" << std::endl;
        std::string path = std::string(1, SYSTEM_SEPARATOR).append(inodes[inode].name);
        unsigned long dir = curr_dir;
        while (dir != 0) {
            path = std::string(1, SYSTEM_SEPARATOR).append(inodes[dir].name).append(path);
            dir = inodes[dir].upper_folder;
        }
        std::cout << "Path: " << path << std::endl;
        std::cout << "Size: " << inodes[inode].file_size * BLOCK_SIZE << " B" << std::endl;  // with current implementation, size is actually double the value
        std::cout << std::endl;
    }
    
    void directory_info(unsigned long inode) {
        std::cout << "Directory \"" << inodes[inode].name << "\"" << std::endl;
        std::string path = std::string(1, SYSTEM_SEPARATOR);
        unsigned long dir = inode;
        while (dir != 0) {
            path = std::string(1, SYSTEM_SEPARATOR).append(inodes[dir].name).append(path);
            dir = inodes[dir].upper_folder;
        }
        std::cout << "Path: " << path << std::endl;
        std::cout << "Size: " << inodes[inode].file_size * BLOCK_SIZE << " B" << std::endl;  // with current implementation, size is actually double the value
        std::cout << "Files (" << get_block_by_inode(inode)->files_amount << ") ";
        for (long i=0; i<get_block_by_inode(inode)->files_amount; i++) {
            std::cout << inodes[get_block_by_inode(inode)->files[i]].name << " ";
        }
        std::cout << std::endl << std::endl;
    }
    
    void edit_file(unsigned long inode, std::string new_name) {
        strcpy(inodes[inode].name, new_name.c_str());
    }

public:
    FileSystem(unsigned long n) {
    // mkfs
        inode_amount = n;
        block_amount = n;
        inode_map = new MapByte[n / 8 + 1];
        block_map = new MapByte[n / 8 + 1];
        inodes = new INode[n];
        blocks = new Block[n];
        for (unsigned long i=0; i<n/8+1; i++) {
            for (unsigned short j=0; j<8; j++) {
                inode_map[i].assign(j, 0);
                block_map[i].assign(j, 0);
            }
        }
        // create first, root directory
        block_map[0].a = 1;
        inodes[0].upper_folder = 0;
        inodes[0].file_ptr = 0;
        inodes[0].file_size = 1;
        inodes[0].file_type = 'd';
        strcpy(inodes[0].name, ROOT_DIR);
        inode_map[0].a = 1;
        curr_dir = 0;
    }
    
    FileSystem(std::string filename) {
        std::ifstream file;
        unsigned char temp;
        file.open(filename);
        file >> inode_amount;
        file >> block_amount;
        inode_map = new MapByte[inode_amount / 8 + 1];
        block_map = new MapByte[block_amount / 8 + 1];
        for (unsigned long i=0; i<inode_amount/8+1; i++) {
            for (unsigned short j=0; j<8; j++) {
                file >> temp;
                inode_map[i].assign(j, ((temp & ( 1 << j )) >> j));
            }
        }
        for (unsigned long i=0; i<block_amount/8+1; i++) {
            for (unsigned short j=0; j<8; j++) {
                file >> temp;
                block_map[i].assign(j, ((temp & ( 1 << j )) >> j));
            }
        }
        inodes = new INode[inode_amount];
        blocks = new Block[block_amount];
        for (unsigned long i=0; i<inode_amount; i++) {
            file >> inodes[i].upper_folder;
            file >> inodes[i].file_ptr;
            file >> inodes[i].file_size;
            file >> inodes[i].file_type;
            file >> inodes[i].name;
        }
        for (unsigned long i=0; i<block_amount; i++) {
            file >> blocks[i].data;
            file >> blocks[i].files_amount;
            for (long j=0; j<(long)(blocks[i].files_amount)-1; j++) {
                file >> blocks[i].files[j];
            }
            file >> blocks[i].files[blocks[i].files_amount-1];
        }
        curr_dir = 0;
        file.close();
    }
    
    ~FileSystem() {
    // rmfs
        delete[] inode_map;
        delete[] block_map;
        delete[] inodes;
        delete[] blocks;
    }
    
    void save(std::string name="root") {
        std::ofstream file;
        file.open(name.append(".filesystem"));
        file << inode_amount << std::endl << block_amount << std::endl;
        for (unsigned long i=0; i<inode_amount/8+1; i++) {
            for (unsigned short j=0; j<8; j++) {
                file << inode_map[i][j];
            }
            file << std::endl;
        }
        for (unsigned long i=0; i<block_amount/8+1; i++) {
            for (unsigned short j=0; j<8; j++) {
                file << block_map[i][j];
            }
            file << std::endl;
        }
        for (unsigned long i=0; i<inode_amount; i++) {
            file << inodes[i].upper_folder << " " << inodes[i].file_ptr << " " << inodes[i].file_size << " ";
            if (inodes[i].file_type == 'd' || inodes[i].file_type == 'f' || inodes[i].file_type == 's')
                file << inodes[i].file_type << " ";
            else
                file << '0' << " ";
            if (strlen(inodes[i].name) == 0)
                file << "foo" << std::endl;
            else
                file << inodes[i].name << std::endl;
        }
        for (unsigned long i=0; i<block_amount; i++) {
            if (strlen(blocks[i].data) == 0)
                file << "foo" << std::endl;
            else
                file << blocks[i].data << std::endl;
            file << blocks[i].files_amount << std::endl;
            for (long j=0; j<(long)(blocks[i].files_amount)-1; j++)
                file << blocks[i].files[j] << " ";
            file << blocks[i].files[blocks[i].files_amount-1] << std::endl;
        }
        file.close();
    }
    
    void info() {
    // szfs
        unsigned long actual_curr_dir = curr_dir;
        curr_dir = 0;
        directory_info();
        curr_dir = actual_curr_dir;
    }
    
    void make_file(std::string name, unsigned long size=1) {
    // mk
        unsigned long b = alloc_blocks(size);
        unsigned long n = alloc_node();
        
        set_block(b, n, curr_dir, size, 'f', name);
        strcpy(get_block_by_inode(n)->data, std::string(BLOCK_SIZE, '0').c_str());
        
        Block* bdir = get_block_by_inode(curr_dir);
        bdir->files[bdir->files_amount++] = n;
        unsigned long dir = curr_dir;
        inodes[dir].file_size += size;
        while (dir != 0) {
            dir = inodes[dir].upper_folder;
            inodes[dir].file_size += size;
        }
    }
    
    void make_directory(std::string name, unsigned long size=1) {
    // mkdir
        unsigned long b = alloc_blocks(size);
        unsigned long n = alloc_node();
        
        set_block(b, n, curr_dir, size, 'd', name);
        
        Block* bdir = get_block_by_inode(curr_dir);
        bdir->files[bdir->files_amount++] = n;
        unsigned long dir = curr_dir;
        inodes[dir].file_size += size;
        while (dir != 0) {
            dir = inodes[dir].upper_folder;
            inodes[dir].file_size += size;
        }
    }
    
    void delete_file(std::string name) {
    // rm
        do_delete(get_inode_by_name(name));
    }
    
    void delete_directory(std::string name) {
    // rmdir
        do_delete_recursive(get_inode_by_name(name));
    }
    
    void file_info(std::string name) {
    // sz
        file_info(get_inode_by_name(name));
    }
    
    void directory_info() {
    // szdir
        directory_info(curr_dir);
    }
    
    void copy_file(std::string source, std::string destination) {
    // cp
        // TODO copy from outside fs
        unsigned long s = get_inode_by_name(source);
        unsigned long d = get_inode_by_name(destination);
        
        assert(inodes[s].file_type == 'f');
        
        unsigned long b = alloc_blocks(inodes[s].file_size);
        unsigned long n = alloc_node();
        
        set_block(b, n, inodes[d].upper_folder, inodes[s].file_size, 'f', inodes[s].name);
        strcpy(get_block_by_inode(d)->data, get_block_by_inode(s)->data);
        
        Block* bdir = get_block_by_inode(d);
        bdir->files[bdir->files_amount++] = n;
        unsigned long dir = d;
        inodes[dir].file_size += inodes[s].file_size;
        while (dir != 0) {
            dir = inodes[dir].upper_folder;
            inodes[dir].file_size += inodes[s].file_size;
        }
    }
    
    void copy_file_shallow(std::string source, std::string destination) {
    // cp
        // TODO copy from outside fs
        unsigned long s = get_inode_by_name(source);
        unsigned long d = get_inode_by_name(destination);
        
        assert(inodes[s].file_type == 'f');
        
        unsigned long n = alloc_node();
        
        set_block(inodes[s].file_ptr, n, inodes[d].upper_folder, 1, 's', inodes[s].name);
        
        Block* bdir = get_block_by_inode(d);
        bdir->files[bdir->files_amount++] = n;
        unsigned long dir = d;
        inodes[dir].file_size += 1;
        while (dir != 0) {
            dir = inodes[dir].upper_folder;
            inodes[dir].file_size += 1;
        }
    }
    
    void move_file(std::string source, std::string destination) {
    // mv
        copy_file(source, destination);
        delete_file(source);
    }
    
    void edit_file(std::string file_name, std::string new_name) {
    // ed
        edit_file(get_inode_by_name(file_name), new_name);
    }
    
    void change_directory(std::string name) {
    // go
        curr_dir = get_inode_by_name(name);
    }
};

int main(int argc, const char * argv[]) {
//    FileSystem fs("root.filesystem");
//    fs.info();
//    fs.change_directory("test1");
//    fs.directory_info();
//    fs.change_directory("test2");
//    fs.directory_info();
    FileSystem fs(1024);
//    fs.make_file("test", 5);
//    fs.info();
//    fs.file_info("test");
    fs.make_directory("test1");
    fs.change_directory("test1");
    fs.make_directory("test2");
    fs.change_directory("test2");
    fs.directory_info();
    fs.make_file("test3", 3);
    fs.copy_file("test3", "..");
    fs.copy_file_shallow("test3", "../..");
    fs.directory_info();
    fs.change_directory("..");
    fs.directory_info();
    fs.info();
    return 0;
}
