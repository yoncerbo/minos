#ifndef INCLUDE_PLIC
#define INCLUDE_PLIC

inline void plic_enable(uint32_t id);
inline void plic_set_priority(uint32_t id, uint8_t priority);
inline void plic_set_threshold(uint8_t threshold);
inline uint32_t plic_claim(void);
inline void plic_complete(uint32_t id);
inline void plic_enablep(uint32_t id, uint8_t priority);

#endif
