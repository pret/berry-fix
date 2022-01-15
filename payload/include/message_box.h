#ifndef GUARD_MESSAGE_BOX_H
#define GUARD_MESSAGE_BOX_H

enum Message
{
    MSG_WILL_NOW_UPDATE = 0,
    MSG_HAS_BEEN_UPDATED,
    MSG_UNABLE_TO_UPDATE,
    MSG_NO_NEED_TO_UPDATE,
    MSG_UPDATING
};

void MessageBox_Load(void);
void MessageBox_Display(enum Message);

#endif //GUARD_MESSAGE_BOX_H
