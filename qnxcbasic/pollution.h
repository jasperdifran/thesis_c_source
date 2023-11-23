/*
 * pollution.h
 *
 *  Created on: 7 Oct 2023
 *      Author: Jasper Di Francesco
 */

#ifndef POLLUTION_H_
#define POLLUTION_H_

#define DACHEPOLLUTION_SIZE 64000

void pollute_dcache(void);
void pollute_icache(void);
void real_cache_func(void);

#endif /* POLLUTION_H_ */
