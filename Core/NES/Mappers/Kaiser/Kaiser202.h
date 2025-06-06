#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/NesConsole.h"
#include "NES/NesCpu.h"

class Kaiser202 : public BaseMapper
{
	uint16_t _irqReloadValue = 0;
	uint16_t _irqCounter = 0;
	uint8_t _irqControl = 0;
	uint8_t _selectedReg = 0;
	uint8_t _prgRegs[4] = {};
	bool _useRom = false;

protected:
	uint16_t GetPrgPageSize() override { return 0x2000; }
	uint16_t GetChrPageSize() override { return 0x0400; }
	bool EnableCpuClockHook() override { return true; }

	void InitMapper() override
	{
		_irqReloadValue = 0;
		_irqCounter = 0;
		_irqControl = 0;
		_selectedReg = 0;
		memset(_prgRegs, 0, sizeof(_prgRegs));
		
		SelectPrgPage(3, -1);
	}

	void Serialize(Serializer& s) override
	{
		BaseMapper::Serialize(s);
		SV(_irqReloadValue);
		SV(_irqCounter);
		SV(_irqControl);
		SV(_selectedReg);
		SV(_useRom);
		SVArray(_prgRegs, 4);
	}

	void ProcessCpuClock() override
	{
		BaseProcessCpuClock();

		if(_irqControl & 0x02) {
			_irqCounter++;
			if(_irqCounter == 0xFFFF) {
				_irqCounter = _irqReloadValue;
				_irqControl &= ~0x02;
				_console->GetCpu()->SetIrqSource(IRQSource::External);
			}
		}
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		switch(addr & 0xF000) {
			case 0x8000: _irqReloadValue = (_irqReloadValue & 0xFFF0) | (value & 0x0F); break;
			case 0x9000: _irqReloadValue = (_irqReloadValue & 0xFF0F) | ((value & 0x0F) << 4); break;
			case 0xA000: _irqReloadValue = (_irqReloadValue & 0xF0FF) | ((value & 0x0F) << 8); break;
			case 0xB000: _irqReloadValue = (_irqReloadValue & 0x0FFF) | ((value & 0x0F) << 12); break;
			
			case 0xC000: 
				_irqControl = value;
				if(_irqControl & 0x02) {
					_irqCounter = _irqReloadValue;
				}
				_console->GetCpu()->ClearIrqSource(IRQSource::External);
				break;

			case 0xD000: _console->GetCpu()->ClearIrqSource(IRQSource::External); break;
			case 0xE000: _selectedReg = (value & 0x07) - 1; break;

			case 0xF000: 
				switch(_selectedReg) {
					case 0: case 1: case 2: case 3:
						_prgRegs[_selectedReg] = ((_prgRegs[_selectedReg]) & 0x10) | (value & 0x0F);
						break;

					case 4:
						_useRom = value & 0x04;
						break;
				}

				if(_romInfo.MapperID == 56) {
					switch(addr & 0xFC00) {
						case 0xF000: {
							uint8_t bank = addr & 0x03;
							_prgRegs[bank] = (value & 0x10) | (_prgRegs[bank] & 0x0F);
							break;
						}

						case 0xF800:
							SetMirroringType(value & 0x01 ? MirroringType::Vertical : MirroringType::Horizontal);
							break;

						case 0xFC00:
							SelectChrPage(addr & 0x07, value);
							break;
					}
				}

				if(_useRom) {
					SetCpuMemoryMapping(0x6000, 0x7FFF, _prgRegs[3], PrgMemoryType::PrgRom, MemoryAccessType::Read);
				} else {
					SetCpuMemoryMapping(0x6000, 0x7FFF, 0, PrgMemoryType::WorkRam, MemoryAccessType::ReadWrite);
				}

				SelectPrgPage(0, _prgRegs[0]);
				SelectPrgPage(1, _prgRegs[1]);
				SelectPrgPage(2, _prgRegs[2]);
				break;
		}
	}
};