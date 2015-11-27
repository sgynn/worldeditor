#ifndef _UNDO_
#define _UNDO_


class UndoCommand {
	public:
	virtual ~UndoCommand() {}
	virtual void execute() = 0;
	virtual const char* getName() const = 0;
};

#endif

