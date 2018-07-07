#ifndef _STR_TOOLS_H_
#define _STR_TOOLS_H_

const char* valid_chars = ",./\\~!@#$%%^&*()_-+=<>?;:'""|`[]{}0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

void clear_string(char* str)
{
	char *src, *dst;
	for(src = dst = str; *src != '\0'; ++src)
	{
		if(strchr(valid_chars, *src))
		{
			*dst = *src;
			++dst;
		}
	}
	*dst = '\0';
}

bool only_digits(char *s)
{
	char c;
	while(*s)
	{
		c = *s++;
		int r = isdigit(c);
		if(r==0)
			return false;
	}
	return true;
}

const char* valid_digit_chars = "-,0123456789";
String at_str_digits(char* str, int len)
{
	String result;
	int i = 0;
	char c = str[i];
	while(i < len && c != '\r')
	{
		if(strchr(valid_digit_chars, c))
			result += c;
		i++;
		c = str[i];
	}
	return result;
}

int strtoint(String str, char delimiter, int* args)
{
	int i = 0, idx0 = str.indexOf(delimiter), idx1 = -1;
	args[i] = str.substring(0, idx0).toInt();
	while(idx0 > -1)
	{
		idx1 = str.indexOf(delimiter, idx0+1);
		i++;
		if(idx1 > -1)
			args[i] = str.substring(idx0+1, idx1).toInt();
		else break;
		idx0 = idx1;
	}
	if(str.length()-1 > idx0)
		args[i] = str.substring(idx0+1, str.length()).toInt();
	return i+1;
}

#endif
