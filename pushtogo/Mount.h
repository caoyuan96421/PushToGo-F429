#ifndef MOUNT_H_
#define MOUNT_H_

typedef enum
{
	MOUNT_STOPPED = 0,
	MOUNT_SLEWING = 1,
	MOUNT_TRACKING = 2,
	MOUNT_NUDGING = 4,
	MOUNT_NUDGING_TRACKING = MOUNT_TRACKING | MOUNT_NUDGING
} mountstatus_t;

class Mount
{
protected:
	mountstatus_t status;

public:
	Mount() :
			status(MOUNT_STOPPED)
	{
	}

	virtual ~Mount()
	{
	}

	mountstatus_t getStatus()
	{
		return status;
	}
};

#endif /*MOUNT_H_*/
