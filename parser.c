#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser.h"

struct Stack
{
    void *head;
    void *tail;
    int size;
    size_t data_size;
};

static struct Stack *create_new_stack(size_t data_size, int default_size)
{
    struct Stack *stack = malloc(sizeof(struct Stack));

    stack->data_size = data_size;

    stack->head = malloc(sizeof(data_size) * default_size);
    stack->tail = stack->head;
}

static void free_stack(struct Stack *stack)
{
    free(stack->head);
    free(stack);
}

static void push_into_stack(void *source, struct Stack *stack)
{
    unsigned char *ptr_source = (unsigned char*)source;
    unsigned char *tail = (unsigned char*)stack->tail;

    for(int i = 0; i < stack->data_size; i++)
    {
        tail[i] = ptr_source[i];
    }

    stack->tail += stack->data_size;
    stack->size++;
}

static void *get_value_from_stack(struct Stack *stack, int index)
{
    unsigned char *ptr;
    unsigned char *tail = (unsigned char*)stack->tail;

    ptr = tail - ((index + 1) * stack->data_size);

    return (void *)ptr;
}

static void *pop_out_the_stack(struct Stack *stack)
{
    unsigned char *ptr;
    unsigned char *tail = (unsigned char*)stack->tail;

    ptr = tail - stack->data_size;

    stack->tail = (void*)ptr;
    stack->size--;

    return stack->tail;
}

void empty_stack(struct Stack *stack)
{
    while (stack->size > 0)
    {
        pop_out_the_stack(stack);
    }  
}


enum Type_flag { NEW_ARRAY, NEW_OBJ, CLOSE_ARRAY, CLOSE_OBJ, NULL_FLAG };
enum Level_flag { INCREASE, DECREASE, STEADY };

static void is_it_new_array(char a, enum Type_flag *flag)
{
    if(a == '[') *flag = NEW_ARRAY;
}

static void is_it_new_obj(char a, enum Type_flag *flag)
{
    if(a == '{') *flag = NEW_OBJ;
}

static void is_it_close_array(char a, char b, enum Type_flag *flag)
{
    if(a == ']' && b == '[') *flag = CLOSE_ARRAY;
}

static void is_it_close_obj(char a, char b, enum Type_flag *flag)
{
    if(a == '}' && b == '{') *flag = CLOSE_OBJ;
}

static enum Type_flag analyse_stack_type(struct Stack *stack)
{
    enum Type_flag flag = NULL_FLAG;

    char a = *(char*)get_value_from_stack(stack,0);
    is_it_new_array(a, &flag);
    is_it_new_obj(a, &flag);

    if(stack->size > 1)
    {
        char b = *(char*)get_value_from_stack(stack,1);
        is_it_close_array(a, b, &flag);
        is_it_close_obj(a, b, &flag);
    }

    return flag;
}

static void update_stack_type(enum Type_flag flag, struct Stack *stack)
{
    if(flag == CLOSE_ARRAY || flag == CLOSE_OBJ) 
    {
        for(int i = 0; i < 2; i++)
        {
            pop_out_the_stack(stack);
        }
    }
}

static int push_into_stack_type(char c, struct Stack *stack)
{
    int has_pushed_into_stack = 0;

    switch (c)
    {
        case '[':
        case ']':
        case '{':
        case '}':
            push_into_stack(&c, stack);
            has_pushed_into_stack = 1;
            break;
        
        default:
            break;
        }

    return has_pushed_into_stack;
}

static enum Level_flag analyse_type_flag_to_get_level_flag_accordingly(enum Type_flag type_flag)
{
    enum Level_flag level_flag = STEADY;

    switch (type_flag)
    {
        case NEW_ARRAY:
        case NEW_OBJ:
            level_flag = INCREASE;
            break;

        case CLOSE_ARRAY:
        case CLOSE_OBJ:
            level_flag = DECREASE;
            break;
        
        default:
            level_flag = STEADY;
            break;
    }

    return level_flag;
}

static enum Type_flag push_analyse_and_update_stack_type(char c, struct Stack *stack)
{
    int has_pushed_into_stack = push_into_stack_type(c, stack);

    if(!has_pushed_into_stack) return NULL_FLAG;

    enum Type_flag flag = analyse_stack_type(stack);

    update_stack_type(flag,stack);

    return flag;
}

enum JSON_Type { ARRAY, OBJECT, NULL_FLAG_TYPE };

static void update_current_json_type_according_to_the_type_flag(enum Type_flag flag, enum JSON_Type *current_type)
{
    switch (flag)
    {
    case NEW_ARRAY:
        *current_type = ARRAY;
        break;

    case NEW_OBJ:
        *current_type = OBJECT;
        break;
    
    default:
        break;
    }
}

static void update_level_according_to_the_level_flag(enum Level_flag flag, int *level)
{
    switch (flag)
    {
    case INCREASE:
        *level = *level+1;
        break;

    case DECREASE:
        *level = *level-1;
        break;
    
    default:
        break;
    }
}

static int update_stack_count_according_to_level_flag(char c, enum Level_flag level_flag, int *buffer_state, struct Stack *stack)
{
    switch (level_flag)
    {
    case INCREASE:
    int i = 0;
        push_into_stack(&i,stack);
        break;

    case DECREASE:
        pop_out_the_stack(stack);
        break;

    case STEADY:
        if(c == ',' && *buffer_state == 0) 
        {
            int *top = (int*)get_value_from_stack(stack, 0);
            *top = *top + 1;
        }
        break;
    
    default:
        break;
    }

    return *(int*)get_value_from_stack(stack, 0);
}

static int update_buffer(char c, char *buffer, int *ptr_buffer, int *buffer_state, char *discard)
{
    int has_discard_buffer = 0;

    if(c == '"')
    {
        *buffer_state = *buffer_state == 1 ? 0 : 1;

        if(*buffer_state == 0)
        {
            memset(discard,0,1000);
            memcpy(discard, buffer, *ptr_buffer);
            has_discard_buffer = 1;
        }
    }

    switch (*buffer_state)
    {
    case 1:
        if(c == '"') return has_discard_buffer;
        buffer[*ptr_buffer] = c;
        *ptr_buffer = *ptr_buffer + 1;
        break;
    
    case 0:
        *ptr_buffer = 0;
        break;
    
    default:
        break;
    }

    return has_discard_buffer;
}

static int extract_index_from_array_key(char *key)
{
    char i[100];

    char c = key[0];
    int index = 0;
    int ptr = 0;

    while (c != ']')
    {
        c = key[ptr];

        if(c != '[')
        {
            i[index] = key[ptr];
            index++;
        }

        ptr++;
    }
    int result = atoi(i);

    return result;
}

static int handle_array_key(char *key, int current_key_index, int current_count, int current_level)
{
    int index = extract_index_from_array_key(key);

    if(index == current_count && current_key_index +1 == current_level) return 1;
    
    return 0;
}

static int handle_object_key(char c, char *key, char *discard, int current_key_index, int current_level)
{
    if(c == ':' && current_key_index +1 == current_level && strcmp(key,discard) == 0) return 1;
    return 0;
}

static int update_current_key(char **key_array, int size, char *current_key, int *current_key_index, enum JSON_Type *current_type_key)
{
    *current_key_index = *current_key_index + 1;

    if(*current_key_index >= size) return 0;

    char *key = key_array[*current_key_index];

   *current_type_key = key[0] == '[' ? ARRAY : OBJECT;

   if(strlen(key) + 1 > 50){ printf("Key to big, need to be less than 50 char...\n"); exit(1); }

    strcpy(current_key, key);

    return 1;
}

/* TODO: refactor code */

char *parse(char **key_array, int size, FILE *file_r)
{
    fseek(file_r, 0, SEEK_SET);

    enum JSON_Type current_json_type = NULL_FLAG_TYPE;
    int current_level = 0;

    char current_key[50];
    int current_key_index = -1;
    enum JSON_Type current_type_key = NULL_FLAG_TYPE;

    update_current_key(key_array, size, current_key, &current_key_index, &current_type_key);

    struct Stack *stack_type = create_new_stack(sizeof(char), 100);
    struct Stack *stack_count = create_new_stack(sizeof(int), 100);

    char buffer[1000];
    int ptr_buffer = 0;
    int buffer_state = 0;
    char *discard = malloc(1000);

    char *result = malloc(1000);
    int ptr_result = 0;

    int wait_for_result = 0;
    int result_is_string = 0;

    int debug = 0;

    while (!feof(file_r))
    {
        char c = fgetc(file_r);
        
        /* TODO: refactor code */

        enum Type_flag type_flag = push_analyse_and_update_stack_type(c, stack_type);

        enum Level_flag level_flag = analyse_type_flag_to_get_level_flag_accordingly(type_flag);

        update_current_json_type_according_to_the_type_flag(type_flag, &current_json_type);

        update_level_according_to_the_level_flag(level_flag, &current_level);

        int current_count = update_stack_count_according_to_level_flag(c, level_flag, &buffer_state,stack_count);

        int has_discard_buffer = update_buffer(c, buffer, &ptr_buffer, &buffer_state, discard);

        int is_found_key = 0;

        if(current_type_key == ARRAY) is_found_key = handle_array_key(current_key, current_key_index, current_count, current_level);
        else is_found_key =  handle_object_key(c, current_key, discard, current_key_index, current_level);
        
        if(is_found_key)
        {
            int result = update_current_key(key_array, size, current_key, &current_key_index, &current_type_key);

            if(result == 0) wait_for_result = 1;
        } 

        if(wait_for_result == 1)
        {
            if(c == '"') result_is_string = 1;
            if(c == ',' && current_type_key == ARRAY && debug == 0) debug++; //problem because update count on the , directly. The problem is just for array due to his depedency to current_count for the answer.

            else
            {
                if(!result_is_string && c != ' ' && c != ':' && c != '\n')
                {
                    if(c == ',' || c == ']' || c == '}') return result;

                    result[ptr_result] = c;

                    ptr_result = ptr_result + 1;
                }


                if(result_is_string && has_discard_buffer == 1) return discard;
            }
        }
    }

    printf("key not found ");
    printf("%s...\n",current_key);

    return NULL;
}
