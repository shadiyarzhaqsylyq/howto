/*
Uses R0,R1...,R12, R0 is 1 table, R1 is 2 table, R2 is 3 table
{R0,R1,R2} is 2^3 = 8 entries(subsets)
R0 = 1 << 0, bit 0, 2^0 = 1
R1 = 1 << 1, bit 1, 2^1 = 2
R2 = 1 << 2, bit 2, 2^2 = 4
R11 = 1 << 11, bit 11, 2^12 = 4096
{}
{R0}
{R1}
{R0,R1}
{R2}
{R0,R2}
{R1,R2}
{R0,R1,R2}
3 tables have 8 entries, {} empty set does not count
*/
