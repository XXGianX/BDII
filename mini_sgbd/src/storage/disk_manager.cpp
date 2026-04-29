#include "disk_manager.h"
#include <fcntl.h>      // open, O_RDWR, O_CREAT
#include <unistd.h>     // read, write, lseek, fsync, close
#include <sys/stat.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

DiskManager::DiskManager(const std::string& db_filename)
    : filename_(db_filename), num_pages_(0)
{
    // O_CREAT | O_RDWR: crea si no existe, abre en lectura/escritura
    // 0644: permisos rw-r--r--
    fd_ = open(db_filename.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("No se pudo abrir el archivo: " + db_filename);
    }

    // Calcular cuántas páginas ya existen en el archivo
    struct stat st;
    fstat(fd_, &st);
    num_pages_ = st.st_size / PAGE_SIZE;
}

DiskManager::~DiskManager() {
    if (fd_ >= 0) close(fd_);
}

bool DiskManager::read_page(uint32_t page_id, char* page_data) {
    // Calcular offset: page_id * PAGE_SIZE bytes desde el inicio
    off_t offset = static_cast<off_t>(page_id) * PAGE_SIZE;

    if (lseek(fd_, offset, SEEK_SET) < 0) return false;

    ssize_t bytes_read = read(fd_, page_data, PAGE_SIZE);
    if (bytes_read != static_cast<ssize_t>(PAGE_SIZE)) {
        // Si la página aún no existe, devolvemos ceros
        memset(page_data, 0, PAGE_SIZE);
    }
    return true;
}

bool DiskManager::write_page(uint32_t page_id, const char* page_data) {
    off_t offset = static_cast<off_t>(page_id) * PAGE_SIZE;

    if (lseek(fd_, offset, SEEK_SET) < 0) return false;

    ssize_t bytes_written = write(fd_, page_data, PAGE_SIZE);
    if (bytes_written != static_cast<ssize_t>(PAGE_SIZE)) return false;

    // fsync garantiza que los datos llegan al disco físico
    // Sin esto, el SO podría mantenerlos en caché y perderse ante un crash
    if (fsync(fd_) != 0) return false;

    if (page_id >= num_pages_) {
        num_pages_ = page_id + 1;
    }
    return true;
}

bool DiskManager::write_atomic(const char* data, size_t size) {
    // Patrón: escribir en archivo temporal → fsync → rename
    // Si el proceso muere antes del rename, el archivo original queda intacto
    std::string tmp_path = filename_ + ".tmp";

    int tmp_fd = open(tmp_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (tmp_fd < 0) return false;

    // 1. Escribir todos los datos al archivo temporal
    ssize_t written = write(tmp_fd, data, size);
    if (written != static_cast<ssize_t>(size)) {
        close(tmp_fd);
        return false;
    }

    // 2. fsync: fuerza que los bytes lleguen al disco antes de seguir
    if (fsync(tmp_fd) != 0) {
        close(tmp_fd);
        return false;
    }
    close(tmp_fd);

    // 3. rename es atómico en sistemas POSIX: o reemplaza completamente
    //    o no hace nada. Nunca deja un archivo a medias.
    if (rename(tmp_path.c_str(), filename_.c_str()) != 0) return false;

    return true;
}