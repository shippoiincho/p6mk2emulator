#ifdef __cplusplus
extern "C" {
#endif
    void ym2203_init(uint32_t);
    void ym2203_reset(void);
    void ym2203_write(uint8_t,uint8_t);
    uint8_t ym2203_read(uint8_t);
    uint8_t ym2203_read_status();
    int32_t ym2203_process(void);
    void ym2203_fillbuffer(int16_t *buffer);
    void ym2203_count(uint32_t timer);
#ifdef __cplusplus
}
#endif