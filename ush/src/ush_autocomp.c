#include "ush_internal.h"
#include "ush_config.h"
#include "ush_utils.h"

#include <string.h>

void ush_autocomp_start(struct ush_object *self)
{
        USH_ASSERT(self != NULL);

        self->state = USH_STATE_AUTOCOMP_PREPARE;
}        

bool ush_autocomp_service(struct ush_object *self)
{
        USH_ASSERT(self != NULL);

        bool processed = true;

        switch (self->state) {

        case USH_STATE_AUTOCOMP_PREPARE:
                self->autocomp_input = ush_utils_get_last_arg(self->desc->input_buffer);
                if (self->autocomp_input == NULL) {
                        self->state = USH_STATE_AUTOCOMP_PROMPT_PREPARE;
                } else {
                        self->state = USH_STATE_AUTOCOMP_CANDIDATES_START;
                        self->process_node = self->commands;
                        self->process_index = 0;
                        self->process_index_item = 0;
                }
                break;

        case USH_STATE_AUTOCOMP_CANDIDATES_START:
                self->autocomp_count = 0;
                self->autocomp_prev_count = 0;
                self->state = USH_STATE_AUTOCOMP_CANDIDATES_COUNT;
                self->process_node = self->commands;
                self->process_index = 0;
                self->process_index_item = 0;
                break;

        case USH_STATE_AUTOCOMP_CANDIDATES_COUNT:
        case USH_STATE_AUTOCOMP_CANDIDATES_OPTIMISE:
        case USH_STATE_AUTOCOMP_CANDIDATES_PRINT: {
                if (self->process_node == NULL) {
                        self->autocomp_prev_state = self->state;
                        self->state = USH_STATE_AUTOCOMP_CANDIDATES_FINISH;
                        break;
                }

                if (self->process_index >= self->process_node->file_list_size) {
                        self->process_index = 0;
                        self->process_node = self->process_node->next;
                        break;
                }
                
                struct ush_file_descriptor const *file = &self->process_node->file_list[self->process_index];

                if (ush_utils_startswith((char*)file->name, self->autocomp_input) == false) {
                        self->process_index++;
                        self->process_index_item = 0;
                        break;
                }

                switch (self->process_index_item) {
                case 0:
                        if (self->state == USH_STATE_AUTOCOMP_CANDIDATES_PRINT)
                                ush_write_pointer(self, (char*)file->name, self->state);
                        self->process_index_item = 1;
                        self->autocomp_count++;
                        self->autocomp_candidate_name = (char*)file->name;
                        break;
                case 1:
                        if (self->state == USH_STATE_AUTOCOMP_CANDIDATES_PRINT)
                                ush_write_pointer(self, "\r\n", self->state);
                        self->process_index_item = 2;
                        break;
                case 2:
                        self->process_index++;
                        self->process_index_item = 0;
                        break;
                default:
                        USH_ASSERT(false);
                        break;
                }
                break;
        }

        case USH_STATE_AUTOCOMP_CANDIDATES_FINISH:

                if (self->autocomp_prev_state == USH_STATE_AUTOCOMP_CANDIDATES_PRINT) {
                        self->state = USH_STATE_AUTOCOMP_PROMPT;
                        break;
                }

                if (self->autocomp_prev_state == USH_STATE_AUTOCOMP_CANDIDATES_COUNT) {
                        switch (self->autocomp_count) {
                        case 0:
                                self->state = USH_STATE_READ_CHAR;
                                break;
                        case 1: {
                                char *suffix = self->autocomp_input + strlen(self->autocomp_input);
                                strcpy(self->autocomp_input, self->autocomp_candidate_name);
                                self->in_pos = strlen(self->desc->input_buffer);
                                ush_write_pointer(self, suffix, USH_STATE_READ_CHAR);
                                break;
                        }
                        default:
                                self->autocomp_prev_count = self->autocomp_count;

                                self->desc->input_buffer[self->in_pos++] = self->autocomp_candidate_name[strlen(self->autocomp_input)];
                                if (self->in_pos >= self->desc->input_buffer_size)
                                        self->in_pos = 0;
                                self->desc->input_buffer[self->in_pos] = '\0';

                                self->autocomp_suffix_len = 1;
                                self->autocomp_count = 0;
                                self->process_node = self->commands;
                                self->process_index = 0;
                                self->process_index_item = 0;
                                self->state = USH_STATE_AUTOCOMP_CANDIDATES_OPTIMISE;
                                break;
                        }

                } else if (self->autocomp_prev_state == USH_STATE_AUTOCOMP_CANDIDATES_OPTIMISE) {
                        if (self->autocomp_count < self->autocomp_prev_count) {
                                if (self->in_pos > 0)
                                        self->in_pos--;
                                self->desc->input_buffer[self->in_pos] = '\0';
                                self->autocomp_suffix_len--;
                                self->autocomp_count = 0;
                                self->process_node = self->commands;
                                self->process_index = 0;
                                self->process_index_item = 0;

                                char *suffix = self->autocomp_input + strlen(self->autocomp_input) - self->autocomp_suffix_len;
                                if (strlen(suffix) > 0) {
                                        ush_write_pointer(self, suffix, USH_STATE_READ_CHAR);
                                } else {
                                        self->autocomp_count = 0;
                                        self->process_node = self->commands;
                                        self->process_index = 0;
                                        self->process_index_item = 0;
                                        ush_write_pointer(self, "\r\n", USH_STATE_AUTOCOMP_CANDIDATES_PRINT);
                                }
                                break;
                        }

                        self->autocomp_prev_count = self->autocomp_count;

                        self->desc->input_buffer[self->in_pos++] = self->autocomp_candidate_name[strlen(self->autocomp_input)];
                        if (self->in_pos >= self->desc->input_buffer_size)
                                self->in_pos = 0;
                        self->desc->input_buffer[self->in_pos] = '\0';

                        self->autocomp_suffix_len++;
                        self->autocomp_count = 0;
                        self->process_node = self->commands;
                        self->process_index = 0;
                        self->process_index_item = 0;
                        self->state = USH_STATE_AUTOCOMP_CANDIDATES_OPTIMISE;

                } else {
                        USH_ASSERT(false);
                }
                break;

        case USH_STATE_AUTOCOMP_PROMPT_PREPARE:
                ush_write_pointer(self, "\r\n", USH_STATE_AUTOCOMP_PROMPT);
                break;

        case USH_STATE_AUTOCOMP_PROMPT:
                ush_prompt_start(self, USH_STATE_AUTOCOMP_RECALL);
                break;

        case USH_STATE_AUTOCOMP_RECALL:
                ush_write_pointer(self, self->desc->input_buffer, USH_STATE_READ_CHAR);
                break;

        default:
                processed = false;
                break;

        }

        return processed;
}
