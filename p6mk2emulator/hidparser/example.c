/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Selvin
 * 
 */

#include "ch32v20x.h" //include your board
#include "tusb.h"
#include <stdbool.h>
#include "hidparser.h"

#define INVALID_REPORT_ID -1
// means 1/X of half range of analog would be dead zone
#define DEAD_ZONE 4U

static const char *const BUTTON_NAMES[] = {"NONE", "UP", "RIGHT", "DOWN", "LEFT", "A", "B"};
//(hat format, 8 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
static const uint8_t HAT_SWITCH_TO_DIRECTION_BUTTONS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001, 0b0000};

typedef union
{
	struct
	{
		bool up : 1;
		bool right : 1;
		bool down : 1;
		bool left : 1;
		bool button1 : 1;
		bool button2 : 1;
	};
	struct
	{
		uint8_t all_direction : 4;
		uint8_t all_buttons : 2;
	};
	uint8_t value : 8;
} pad_buttons;

HID_ReportInfo_t *info = NULL;
int16_t reportID;
pad_buttons previous = {0};
uint8_t g_dev_addr = 255;
uint8_t g_instance = 0;

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
	uint16_t vid, pid;
	tuh_vid_pid_get(dev_addr, &vid, &pid);
	uint8_t ret = USB_ProcessHIDReport(desc_report, desc_len, &info);
	printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
	printf("VID = %04x, PID = %04x\r\n", vid, pid);

	if(ret == HID_PARSE_Successful)
	{
		g_dev_addr = dev_addr;
		g_instance = instance;
	}
	else
	{
		printf("Error: USB_ProcessHIDReport failed: %d\r\n", ret);
	}
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
	reportID = INVALID_REPORT_ID;
	USB_FreeReportInfo(info);
	info = NULL;
	previous.value = 0;
	g_dev_addr = 255;
}

//called from parser for filtering report items
bool CALLBACK_HIDParser_FilterHIDReportItem(HID_ReportItem_t *const CurrentItem)
{
	if (CurrentItem->ItemType != HID_REPORT_ITEM_In)
		return false;

	if (reportID == INVALID_REPORT_ID)
	{
		reportID = CurrentItem->ReportID;
	}
	switch (CurrentItem->Attributes.Usage.Page)
	{
		case HID_USAGE_PAGE_DESKTOP:
			switch (CurrentItem->Attributes.Usage.Usage)
			{
				case HID_USAGE_DESKTOP_X:
				case HID_USAGE_DESKTOP_Y:
				case HID_USAGE_DESKTOP_HAT_SWITCH:
				case HID_USAGE_DESKTOP_DPAD_UP:
				case HID_USAGE_DESKTOP_DPAD_DOWN:
				case HID_USAGE_DESKTOP_DPAD_LEFT:
				case HID_USAGE_DESKTOP_DPAD_RIGHT:
					return true;
			}
			return false;
		case HID_USAGE_PAGE_BUTTON:
			return true;
	}
	return false;
}

static inline bool USB_GetHIDReportItemInfoWithReportId(const uint8_t *ReportData, HID_ReportItem_t *const ReportItem)
{
	if (ReportItem->ReportID)
	{
		if (ReportItem->ReportID != ReportData[0])
			return false;

		ReportData++;
	}
	return USB_GetHIDReportItemInfo(ReportItem->ReportID, ReportData, ReportItem);
}

void parse_report(uint8_t const *report, uint16_t len)
{
	pad_buttons current = {0};
	current.value = 0;
	HID_ReportItem_t *item = info->FirstReportItem;
	//iterate filtered reports info to match report from data
	while (item)
	{
		if (USB_GetHIDReportItemInfoWithReportId(report, item))
		{
			switch (item->Attributes.Usage.Page)
			{
			case HID_USAGE_PAGE_DESKTOP:
				switch (item->Attributes.Usage.Usage)
				{
				case HID_USAGE_DESKTOP_X:
				{
					uint32_t range_half = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
					uint32_t dead_zone_range = range_half / DEAD_ZONE;
					if (item->Value < (range_half - dead_zone_range))
					{
						current.left |= 1;
					}
					else if (item->Value > (range_half + dead_zone_range))
					{
						current.right |= 1;
					}
				}
				break;
				case HID_USAGE_DESKTOP_Y:
				{
					uint32_t range_half = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
					uint32_t dead_zone_range = range_half / DEAD_ZONE;
					if (item->Value < (range_half - dead_zone_range))
					{
						current.up |= 1;
					}
					else if (item->Value > (range_half + dead_zone_range))
					{
						current.down |= 1;
					}
				}
				break;
				case HID_USAGE_DESKTOP_HAT_SWITCH:
					current.all_direction |= HAT_SWITCH_TO_DIRECTION_BUTTONS[item->Value];
					break;
				case HID_USAGE_DESKTOP_DPAD_UP:
					current.up |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_RIGHT:
					current.right |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_DOWN:
					current.down |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_LEFT:
					current.left |= 1;
					break;
				}
				break;
			case HID_USAGE_PAGE_BUTTON:
			{
				uint8_t usage = item->Attributes.Usage.Usage;
				if (usage == 1)
				{
					if (item->Value)
					{
						current.button1 = 1;
					}
				}
				else if (usage == 2)
				{
					if (item->Value)
					{
						current.button2 = 1;
					}
				}
			}
			break;
			}
		}
		item = item->Next;
	}
	if (previous.value != current.value)
	{
		//of course you can use GPIO here for hiven buttons
		uint8_t value = current.value;
		if (!value)
		{
			printf(BUTTON_NAMES[0]);
		}
		else
		{
			bool first = true;
			for (int i = 1; i <= 6; i++)
			{
				if (value & 1)
				{
					if (first)
					{
						printf("%s", BUTTON_NAMES[i]);
						first = false;
					}
					else
					{
						printf(", %s", BUTTON_NAMES[i]);
					}
				}
				value >>= 1;
			}
		}
		printf("\n");
		previous.value = current.value;
	}
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
	parse_report(report, len);
	g_dev_addr = dev_addr;
	g_instance = instance;
}

int main()
{
	//init board your board
	Delay_Init();
	USART_Printf_Init(115200);
	printf("SystemClk:%d\r\n", (int)SystemCoreClock);
	//init end

	tusb_init();
	while(true)
	{
		tuh_task();
		if(g_dev_addr != 255)
		{
			if (!tuh_hid_receive_report(g_dev_addr, g_instance))
			{
				printf("Error: cannot request to receive report(tuh_hid_report_received_cb)\r\n");
			}
			g_dev_addr = 255;
		}
	}
}