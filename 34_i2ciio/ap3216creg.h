#ifndef     __AP3216C_H
#define     __AP3216C_H

#define     SYSTEM_CONFIGURATION        0x00    /* 配置寄存器       */
#define     INTERRUPT_STATUS            0x01    /* 中断状态寄存器   */
#define     INT_CLEAR_MANNER            0x02    /* 中断清除寄存器   */
#define     IR_DATA_LOW                 0x0a    /* IR数据低字节     */
#define     IR_DATA_HIGH                0x0b    /* IR数据高字节     */
#define     ALS_DATA_LOW                0x0c    /* ALS数据低字节    */
#define     ALS_DATA_HIGH               0x0d    /* ALS数据高字节    */
#define     PS_DATA_LOW                 0x0e    /* PS数据低字节     */
#define     PS_DATA_HIGH                0x0f    /* PS数据高字节     */

#define     ALS_CONFIG                  0X10	/* ALS配置寄存器 */
#define     PS_CONFIG                   0X20	/* PS配置寄存器 */
#define     PS_LEDCONFIG                0X21	/* LED配置寄存器 */

#endif //   __AP3216C_H