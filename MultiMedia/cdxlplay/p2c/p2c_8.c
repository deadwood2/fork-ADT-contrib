/*
**  Planar 2 Chunky routine (C) 2009 Fredrik Wikstrom
**/

static void p2c_8 (const uint16_t *planar, uint32_t plane_size, uint8_t *chunky,
	uint32_t width, uint32_t height)
{
	uint32_t *cp;
	const uint16_t *pp1, *pp2, *pp3, *pp4, *pp5, *pp6, *pp7, *pp8;
	uint32_t cd1, cd2, cd3, cd4;
	uint32_t pd1, pd2, pd3, pd4, pd5, pd6, pd7, pd8;
	uint32_t x, y;
	cp = (uint32_t *)chunky;
	pp1 = planar;
	pp2 = (const uint16_t *)((const uint8_t *)pp1 + plane_size);
	pp3 = (const uint16_t *)((const uint8_t *)pp2 + plane_size);
	pp4 = (const uint16_t *)((const uint8_t *)pp3 + plane_size);
	pp5 = (const uint16_t *)((const uint8_t *)pp4 + plane_size);
	pp6 = (const uint16_t *)((const uint8_t *)pp5 + plane_size);
	pp7 = (const uint16_t *)((const uint8_t *)pp6 + plane_size);
	pp8 = (const uint16_t *)((const uint8_t *)pp7 + plane_size);
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			pd1 = *pp1++;
			pd2 = *pp2++;
			pd3 = *pp3++;
			pd4 = *pp4++;
			pd5 = *pp5++;
			pd6 = *pp6++;
			pd7 = *pp7++;
			pd8 = *pp8++;
			
#ifdef WORDS_BIGENDIAN
			cd1  = ((pd1 << 9 ) & 0x01000000)|((pd2 << 10) & 0x02000000);
			cd1 |= ((pd3 << 11) & 0x04000000)|((pd4 << 12) & 0x08000000);
			cd1 |= ((pd5 << 13) & 0x10000000)|((pd6 << 14) & 0x20000000);
			cd1 |= ((pd7 << 15) & 0x40000000)|((pd8 << 16) & 0x80000000);
			
			cd2  = ((pd1 << 13) & 0x01000000)|((pd2 << 14) & 0x02000000);
			cd2 |= ((pd3 << 15) & 0x04000000)|((pd4 << 16) & 0x08000000);
			cd2 |= ((pd5 << 17) & 0x10000000)|((pd6 << 18) & 0x20000000);
			cd2 |= ((pd7 << 19) & 0x40000000)|((pd8 << 20) & 0x80000000);
			
			cd3  = ((pd1 << 17) & 0x01000000)|((pd2 << 18) & 0x02000000);
			cd3 |= ((pd3 << 19) & 0x04000000)|((pd4 << 20) & 0x08000000);
			cd3 |= ((pd5 << 21) & 0x10000000)|((pd6 << 22) & 0x20000000);
			cd3 |= ((pd7 << 23) & 0x40000000)|((pd8 << 24) & 0x80000000);
			
			cd4  = ((pd1 << 21) & 0x01000000)|((pd2 << 22) & 0x02000000);
			cd4 |= ((pd3 << 23) & 0x04000000)|((pd4 << 24) & 0x08000000);
			cd4 |= ((pd5 << 25) & 0x10000000)|((pd6 << 26) & 0x20000000);
			cd4 |= ((pd7 << 27) & 0x40000000)|((pd8 << 28) & 0x80000000);
			
			cd1 |= ((pd1 << 2 ) & 0x00010000)|((pd2 << 3 ) & 0x00020000);
			cd1 |= ((pd3 << 4 ) & 0x00040000)|((pd4 << 5 ) & 0x00080000);
			cd1 |= ((pd5 << 6 ) & 0x00100000)|((pd6 << 7 ) & 0x00200000);
			cd1 |= ((pd7 << 8 ) & 0x00400000)|((pd8 << 9 ) & 0x00800000);
			
			cd2 |= ((pd1 << 6 ) & 0x00010000)|((pd2 << 7 ) & 0x00020000);
			cd2 |= ((pd3 << 8 ) & 0x00040000)|((pd4 << 9 ) & 0x00080000);
			cd2 |= ((pd5 << 10) & 0x00100000)|((pd6 << 11) & 0x00200000);
			cd2 |= ((pd7 << 12) & 0x00400000)|((pd8 << 13) & 0x00800000);
			
			cd3 |= ((pd1 << 10) & 0x00010000)|((pd2 << 11) & 0x00020000);
			cd3 |= ((pd3 << 12) & 0x00040000)|((pd4 << 13) & 0x00080000);
			cd3 |= ((pd5 << 14) & 0x00100000)|((pd6 << 15) & 0x00200000);
			cd3 |= ((pd7 << 16) & 0x00400000)|((pd8 << 17) & 0x00800000);
			
			cd4 |= ((pd1 << 14) & 0x00010000)|((pd2 << 15) & 0x00020000);
			cd4 |= ((pd3 << 16) & 0x00040000)|((pd4 << 17) & 0x00080000);
			cd4 |= ((pd5 << 18) & 0x00100000)|((pd6 << 19) & 0x00200000);
			cd4 |= ((pd7 << 20) & 0x00400000)|((pd8 << 21) & 0x00800000);
			
			cd1 |= ((pd1 >> 5 ) & 0x00000100)|((pd2 >> 4 ) & 0x00000200);
			cd1 |= ((pd3 >> 3 ) & 0x00000400)|((pd4 >> 2 ) & 0x00000800);
			cd1 |= ((pd5 >> 1 ) & 0x00001000)|((pd6      ) & 0x00002000);
			cd1 |= ((pd7 << 1 ) & 0x00004000)|((pd8 << 2 ) & 0x00008000);
			
			cd2 |= ((pd1 >> 1 ) & 0x00000100)|((pd2      ) & 0x00000200);
			cd2 |= ((pd3 << 1 ) & 0x00000400)|((pd4 << 2 ) & 0x00000800);
			cd2 |= ((pd5 << 3 ) & 0x00001000)|((pd6 << 4 ) & 0x00002000);
			cd2 |= ((pd7 << 5 ) & 0x00004000)|((pd8 << 6 ) & 0x00008000);
			
			cd3 |= ((pd1 << 3 ) & 0x00000100)|((pd2 << 4 ) & 0x00000200);
			cd3 |= ((pd3 << 5 ) & 0x00000400)|((pd4 << 6 ) & 0x00000800);
			cd3 |= ((pd5 << 7 ) & 0x00001000)|((pd6 << 8 ) & 0x00002000);
			cd3 |= ((pd7 << 9 ) & 0x00004000)|((pd8 << 10) & 0x00008000);
			
			cd4 |= ((pd1 << 7 ) & 0x00000100)|((pd2 << 8 ) & 0x00000200);
			cd4 |= ((pd3 << 9 ) & 0x00000400)|((pd4 << 10) & 0x00000800);
			cd4 |= ((pd5 << 11) & 0x00001000)|((pd6 << 12) & 0x00002000);
			cd4 |= ((pd7 << 13) & 0x00004000)|((pd8 << 14) & 0x00008000);
			
			cd1 |= ((pd1 >> 12) & 0x00000001)|((pd2 >> 11) & 0x00000002);
			cd1 |= ((pd3 >> 10) & 0x00000004)|((pd4 >> 9 ) & 0x00000008);
			cd1 |= ((pd5 >> 8 ) & 0x00000010)|((pd6 >> 7 ) & 0x00000020);
			cd1 |= ((pd7 >> 6 ) & 0x00000040)|((pd8 >> 5 ) & 0x00000080);
			
			cd2 |= ((pd1 >> 8 ) & 0x00000001)|((pd2 >> 7 ) & 0x00000002);
			cd2 |= ((pd3 >> 6 ) & 0x00000004)|((pd4 >> 5 ) & 0x00000008);
			cd2 |= ((pd5 >> 4 ) & 0x00000010)|((pd6 >> 3 ) & 0x00000020);
			cd2 |= ((pd7 >> 2 ) & 0x00000040)|((pd8 >> 1 ) & 0x00000080);
			
			cd3 |= ((pd1 >> 4 ) & 0x00000001)|((pd2 >> 3 ) & 0x00000002);
			cd3 |= ((pd3 >> 2 ) & 0x00000004)|((pd4 >> 1 ) & 0x00000008);
			cd3 |= ((pd5      ) & 0x00000010)|((pd6 << 1 ) & 0x00000020);
			cd3 |= ((pd7 << 2 ) & 0x00000040)|((pd8 << 3 ) & 0x00000080);
			
			cd4 |= ((pd1      ) & 0x00000001)|((pd2 << 1 ) & 0x00000002);
			cd4 |= ((pd3 << 2 ) & 0x00000004)|((pd4 << 3 ) & 0x00000008);
			cd4 |= ((pd5 << 4 ) & 0x00000010)|((pd6 << 5 ) & 0x00000020);
			cd4 |= ((pd7 << 6 ) & 0x00000040)|((pd8 << 7 ) & 0x00000080);
#else
			cd1  = ((pd1 >> 7 ) & 0x00000001)|((pd2 >> 6 ) & 0x00000002);
			cd1 |= ((pd3 >> 5 ) & 0x00000004)|((pd4 >> 4 ) & 0x00000008);
			cd1 |= ((pd5 >> 3 ) & 0x00000010)|((pd6 >> 2 ) & 0x00000020);
			cd1 |= ((pd7 >> 1 ) & 0x00000040)|((pd8      ) & 0x00000080);
			
			cd2  = ((pd1 >> 3 ) & 0x00000001)|((pd2 >> 2 ) & 0x00000002);
			cd2 |= ((pd3 >> 1 ) & 0x00000004)|((pd4      ) & 0x00000008);
			cd2 |= ((pd5 << 1 ) & 0x00000010)|((pd6 << 2 ) & 0x00000020);
			cd2 |= ((pd7 << 3 ) & 0x00000040)|((pd8 << 4 ) & 0x00000080);
			
			cd3  = ((pd1 >> 15) & 0x00000001)|((pd2 >> 14) & 0x00000002);
			cd3 |= ((pd3 >> 13) & 0x00000004)|((pd4 >> 12) & 0x00000008);
			cd3 |= ((pd5 >> 11) & 0x00000010)|((pd6 >> 10) & 0x00000020);
			cd3 |= ((pd7 >> 9 ) & 0x00000040)|((pd8 >> 8 ) & 0x00000080);
			
			cd4  = ((pd1 >> 11) & 0x00000001)|((pd2 >> 10) & 0x00000002);
			cd4 |= ((pd3 >> 9 ) & 0x00000004)|((pd4 >> 8 ) & 0x00000008);
			cd4 |= ((pd5 >> 7 ) & 0x00000010)|((pd6 >> 6 ) & 0x00000020);
			cd4 |= ((pd7 >> 5 ) & 0x00000040)|((pd8 >> 4 ) & 0x00000080);
			
			cd1 |= ((pd1 << 2 ) & 0x00000100)|((pd2 << 3 ) & 0x00000200);
			cd1 |= ((pd3 << 4 ) & 0x00000400)|((pd4 << 5 ) & 0x00000800);
			cd1 |= ((pd5 << 6 ) & 0x00001000)|((pd6 << 7 ) & 0x00002000);
			cd1 |= ((pd7 << 8 ) & 0x00004000)|((pd8 << 9 ) & 0x00008000);
			
			cd2 |= ((pd1 << 6 ) & 0x00000100)|((pd2 << 7 ) & 0x00000200);
			cd2 |= ((pd3 << 8 ) & 0x00000400)|((pd4 << 9 ) & 0x00000800);
			cd2 |= ((pd5 << 10) & 0x00001000)|((pd6 << 11) & 0x00002000);
			cd2 |= ((pd7 << 12) & 0x00004000)|((pd8 << 13) & 0x00008000);
			
			cd3 |= ((pd1 >> 6 ) & 0x00000100)|((pd2 >> 5 ) & 0x00000200);
			cd3 |= ((pd3 >> 4 ) & 0x00000400)|((pd4 >> 3 ) & 0x00000800);
			cd3 |= ((pd5 >> 2 ) & 0x00001000)|((pd6 >> 1 ) & 0x00002000);
			cd3 |= ((pd7      ) & 0x00004000)|((pd8 << 1 ) & 0x00008000);
			
			cd4 |= ((pd1 >> 2 ) & 0x00000100)|((pd2 >> 1 ) & 0x00000200);
			cd4 |= ((pd3      ) & 0x00000400)|((pd4 << 1 ) & 0x00000800);
			cd4 |= ((pd5 << 2 ) & 0x00001000)|((pd6 << 3 ) & 0x00002000);
			cd4 |= ((pd7 << 4 ) & 0x00004000)|((pd8 << 5 ) & 0x00008000);
			
			cd1 |= ((pd1 << 11) & 0x00010000)|((pd2 << 12) & 0x00020000);
			cd1 |= ((pd3 << 13) & 0x00040000)|((pd4 << 14) & 0x00080000);
			cd1 |= ((pd5 << 15) & 0x00100000)|((pd6 << 16) & 0x00200000);
			cd1 |= ((pd7 << 17) & 0x00400000)|((pd8 << 18) & 0x00800000);
			
			cd2 |= ((pd1 << 15) & 0x00010000)|((pd2 << 16) & 0x00020000);
			cd2 |= ((pd3 << 17) & 0x00040000)|((pd4 << 18) & 0x00080000);
			cd2 |= ((pd5 << 19) & 0x00100000)|((pd6 << 20) & 0x00200000);
			cd2 |= ((pd7 << 21) & 0x00400000)|((pd8 << 22) & 0x00800000);
			
			cd3 |= ((pd1 << 3 ) & 0x00010000)|((pd2 << 4 ) & 0x00020000);
			cd3 |= ((pd3 << 5 ) & 0x00040000)|((pd4 << 6 ) & 0x00080000);
			cd3 |= ((pd5 << 7 ) & 0x00100000)|((pd6 << 8 ) & 0x00200000);
			cd3 |= ((pd7 << 9 ) & 0x00400000)|((pd8 << 10) & 0x00800000);
			
			cd4 |= ((pd1 << 7 ) & 0x00010000)|((pd2 << 8 ) & 0x00020000);
			cd4 |= ((pd3 << 9 ) & 0x00040000)|((pd4 << 10) & 0x00080000);
			cd4 |= ((pd5 << 11) & 0x00100000)|((pd6 << 12) & 0x00200000);
			cd4 |= ((pd7 << 13) & 0x00400000)|((pd8 << 14) & 0x00800000);
			
			cd1 |= ((pd1 << 20) & 0x01000000)|((pd2 << 21) & 0x02000000);
			cd1 |= ((pd3 << 22) & 0x04000000)|((pd4 << 23) & 0x08000000);
			cd1 |= ((pd5 << 24) & 0x10000000)|((pd6 << 25) & 0x20000000);
			cd1 |= ((pd7 << 26) & 0x40000000)|((pd8 << 27) & 0x80000000);
			
			cd2 |= ((pd1 << 24) & 0x01000000)|((pd2 << 25) & 0x02000000);
			cd2 |= ((pd3 << 26) & 0x04000000)|((pd4 << 27) & 0x08000000);
			cd2 |= ((pd5 << 28) & 0x10000000)|((pd6 << 29) & 0x20000000);
			cd2 |= ((pd7 << 30) & 0x40000000)|((pd8 << 31) & 0x80000000);
			
			cd3 |= ((pd1 << 12) & 0x01000000)|((pd2 << 13) & 0x02000000);
			cd3 |= ((pd3 << 14) & 0x04000000)|((pd4 << 15) & 0x08000000);
			cd3 |= ((pd5 << 16) & 0x10000000)|((pd6 << 17) & 0x20000000);
			cd3 |= ((pd7 << 18) & 0x40000000)|((pd8 << 19) & 0x80000000);
			
			cd4 |= ((pd1 << 16) & 0x01000000)|((pd2 << 17) & 0x02000000);
			cd4 |= ((pd3 << 18) & 0x04000000)|((pd4 << 19) & 0x08000000);
			cd4 |= ((pd5 << 20) & 0x10000000)|((pd6 << 21) & 0x20000000);
			cd4 |= ((pd7 << 22) & 0x40000000)|((pd8 << 23) & 0x80000000);
#endif
			
			*cp++ = cd1;
			*cp++ = cd2;
			*cp++ = cd3;
			*cp++ = cd4;
		}
	}
}