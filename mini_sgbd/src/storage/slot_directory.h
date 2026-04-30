#pragma once
#include "page.h"
#include <cstring>
#include <optional>

class SlotDirectory {
public:
    // Inserta `length` bytes de `record` en la página.
    // Devuelve el SlotId asignado, o -1 si no hay espacio.
    static int insert(Page& page, const char* record, uint16_t length) {
        if (page.free_space() < length + sizeof(SlotEntry)) {
            return -1;  // página llena
        }

        PageHeader* h = page.header();

        // 1. Mover el puntero de espacio libre hacia atrás (los datos
        //    crecen desde el final de la página hacia el centro)
        h->free_space_ptr -= length;

        // 2. Copiar el registro al espacio reservado
        memcpy(page.data_ + h->free_space_ptr, record, length);

        // 3. Buscar un slot libre reciclable (length == 0)
        SlotId sid = h->num_slots;  // por defecto, nuevo slot al final
        for (SlotId i = 0; i < h->num_slots; i++) {
            if (page.get_slot(i)->length == 0) {
                sid = i;
                break;
            }
        }

        // 4. Escribir la entrada en el Slot Directory
        SlotEntry* s = page.get_slot(sid);
        s->offset = h->free_space_ptr;
        s->length = length;

        // Solo incrementar num_slots si usamos un slot nuevo
        if (sid == h->num_slots) {
            h->num_slots++;
        }

        return sid;
    }

    // Recupera el registro del slot `sid`.
    // Devuelve puntero al inicio y escribe la longitud en `out_len`.
    // Devuelve nullptr si el slot no existe o fue eliminado.
    static const char* get(Page& page, SlotId sid, uint16_t& out_len) {
        PageHeader* h = page.header();
        if (sid >= h->num_slots) return nullptr;

        SlotEntry* s = page.get_slot(sid);
        if (s->length == 0) return nullptr;  // eliminado

        out_len = s->length;
        return page.data_ + s->offset;
    }

    // Marca el slot como eliminado (longitud = 0).
    // No compacta el espacio — eso lo haría un VACUUM en un SGBD real.
    static bool remove(Page& page, SlotId sid) {
        PageHeader* h = page.header();
        if (sid >= h->num_slots) return false;

        SlotEntry* s = page.get_slot(sid);
        if (s->length == 0) return false;  // ya estaba eliminado

        s->length = 0;  // marcar como vacío (tombstone)
        s->offset = 0;
        return true;
    }
};