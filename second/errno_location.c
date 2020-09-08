
static int my_errno;
int * __errno_location(void)
{
return &my_errno;
}
