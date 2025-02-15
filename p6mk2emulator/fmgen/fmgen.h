// ---------------------------------------------------------------------------
//	FM Sound Generator
//	Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//	$Id: fmgen.h,v 1.37 2003/08/25 13:33:11 cisc Exp $

#ifndef FM_GEN_H
#define FM_GEN_H

//#include "types.h"

// ---------------------------------------------------------------------------
//	�o�̓T���v���̌^
//
#define FM_SAMPLETYPE	int16				// int16 or int32

// ---------------------------------------------------------------------------
//	�萔���̂P
//	�ÓI�e�[�u���̃T�C�Y

#define FM_LFOBITS		8					// �ύX�s��
#define FM_TLBITS		7

// ---------------------------------------------------------------------------

#define FM_TLENTS		(1 << FM_TLBITS)
#define FM_LFOENTS		(1 << FM_LFOBITS)
#define FM_TLPOS		(FM_TLENTS/4)

//	�T�C���g�̐��x�� 2^(1/256)
#define FM_CLENTS		(0x1000 * 2)	// sin + TL + LFO

// ---------------------------------------------------------------------------

namespace FM
{	
	//	Types ----------------------------------------------------------------
	typedef FM_SAMPLETYPE	Sample;
	typedef int32 			ISample;

	enum OpType { typeN=0, typeM=1 };

	void StoreSample(ISample& dest, int data);

	class Chip;

	//	Operator -------------------------------------------------------------
	class Operator
	{
	public:
		Operator();
		void	SetChip(Chip* chip) { chip_ = chip; }

		static void	MakeTimeTable(uint ratio);
		
		ISample	Calc(ISample in);
		ISample	CalcL(ISample in);
		ISample CalcFB(uint fb);
		ISample CalcFBL(uint fb);
		ISample CalcN(uint noise);
		void	Prepare();
		void	KeyOn();
		void	KeyOff();
		void	Reset();
		void	ResetFB();
		int		IsOn();

		void	SetDT(uint dt);
		void	SetDT2(uint dt2);
		void	SetMULTI(uint multi);
		void	SetTL(uint tl, bool csm);
		void	SetKS(uint ks);
		void	SetAR(uint ar);
		void	SetDR(uint dr);
		void	SetSR(uint sr);
		void	SetRR(uint rr);
		void	SetSL(uint sl);
		void	SetSSGEC(uint ssgec);
		void	SetFNum(uint fnum);
		void	SetDPBN(uint dp, uint bn);
		void	SetMode(bool modulator);
		void	SetAMON(bool on);
		void	SetMS(uint ms);
		void	Mute(bool);
		
//		static void SetAML(uint l);
//		static void SetPML(uint l);

		int		Out() { return out_; }

		int		dbgGetIn2() { return in2_; } 
		void	dbgStopPG() { pg_diff_ = 0; pg_diff_lfo_ = 0; }
		
	private:
		typedef uint32 Counter;
		
		Chip*	chip_;
		ISample	out_, out2_;
		ISample in2_;

	//	Phase Generator ------------------------------------------------------
		uint32	PGCalc();
		uint32	PGCalcL();

		uint	dp_;		// ��P
		uint	detune_;		// Detune
		uint	detune2_;	// DT2
		uint	multiple_;	// Multiple
		uint32	pg_count_;	// Phase ���ݒl
		uint32	pg_diff_;	// Phase �����l
		int32	pg_diff_lfo_;	// Phase �����l >> x

	//	Envelop Generator ---------------------------------------------------
		enum	EGPhase { next, attack, decay, sustain, release, off };
		
		void	EGCalc();
		void	EGStep();
		void	ShiftPhase(EGPhase nextphase);
		void	SSGShiftPhase(int mode);
		void	SetEGRate(uint);
		void	EGUpdate();
		int		FBCalc(int fb);
		ISample LogToLin(uint a);

		
		OpType	type_;		// OP �̎�� (M, N...)
		uint	bn_;		// Block/Note
		int		eg_level_;	// EG �̏o�͒l
		int		eg_level_on_next_phase_;	// ���� eg_phase_ �Ɉڂ�l
		int		eg_count_;		// EG �̎��̕ψڂ܂ł̎���
		int		eg_count_diff_;	// eg_count_ �̍���
		int		eg_out_;		// EG+TL �����킹���o�͒l
		int		tl_out_;		// TL ���̏o�͒l
//		int		pm_depth_;		// PM depth
//		int		am_depth_;		// AM depth
		int		eg_rate_;
		int		eg_curve_count_;
		int		ssg_offset_;
		int		ssg_vector_;
		int		ssg_phase_;


		uint	key_scale_rate_;		// key scale rate
		EGPhase	eg_phase_;
		uint*	ams_;
		uint	ms_;
		
		uint	tl_;			// Total Level	 (0-127)
		uint	tl_latch_;		// Total Level Latch (for CSM mode)
		uint	ar_;			// Attack Rate   (0-63)
		uint	dr_;			// Decay Rate    (0-63)
		uint	sr_;			// Sustain Rate  (0-63)
		uint	sl_;			// Sustain Level (0-127)
		uint	rr_;			// Release Rate  (0-63)
		uint	ks_;			// Keyscale      (0-3)
		uint	ssg_type_;	// SSG-Type Envelop Control

		bool	keyon_;
		bool	amon_;		// enable Amplitude Modulation
		bool	param_changed_;	// �p�����[�^���X�V���ꂽ
		bool	mute_;
		
	//	Tables ---------------------------------------------------------------
		static Counter rate_table[16];
		static uint32 multable[4][16];

		static const uint8 notetable[128];
		static const int8 dttable[256];
		static const int8 decaytable1[64][8];
		static const int decaytable2[16];
		static const int8 attacktable[64][8];
		static const int ssgenvtable[8][2][3][2];

//		static uint	sinetable[1024];
//		static int32 cltable[FM_CLENTS];
		const static uint	sinetable[1024];
		const static int32 cltable[FM_CLENTS];


		static bool tablehasmade;
		static void MakeTable();



	//	friends --------------------------------------------------------------
		friend class Channel4;
//		friend void __stdcall FM_NextPhase(Operator* op);

	public:
		int		dbgopout_;
		int		dbgpgout_;
		static const int32* dbgGetClTable() { return cltable; }
		static const uint* dbgGetSineTable() { return sinetable; }
	};
	
	//	4-op Channel ---------------------------------------------------------
	class Channel4
	{
	public:
		Channel4();
		void SetChip(Chip* chip);
		void SetType(OpType type);
		
		ISample Calc();
		ISample CalcL();
		ISample CalcN(uint noise);
		ISample CalcLN(uint noise);
		void SetFNum(uint fnum);
		void SetFB(uint fb);
		void SetKCKF(uint kc, uint kf);
		void SetAlgorithm(uint algo);
		int Prepare();
		void KeyControl(uint key);
		void Reset();
		void SetMS(uint ms);
		void Mute(bool);
		void Refresh();

		void dbgStopPG() { for (int i=0; i<4; i++) op[i].dbgStopPG(); }
		
	private:
		static const uint8 fbtable[8];
		uint	fb;
		int		buf[4];
		int*	in[3];			// �e OP �̓��̓|�C���^
		int*	out[3];			// �e OP �̏o�̓|�C���^
		int*	pms;
		int		algo_;
		Chip*	chip_;

		static void MakeTable();

		static bool tablehasmade;
		static int 	kftable[64];


	public:
		Operator op[4];
	};

	//	Chip resource
	class Chip
	{
	public:
		Chip();
		void	SetRatio(uint ratio);
		void	SetAML(uint l);
		void	SetPML(uint l);
		void	SetPMV(int pmv) { pmv_ = pmv; }

		uint32	GetMulValue(uint dt2, uint mul) { return multable_[dt2][mul]; }
		uint	GetAML() { return aml_; }
		uint	GetPML() { return pml_; }
		int		GetPMV() { return pmv_; }
		uint	GetRatio() { return ratio_; }

	private:
		void	MakeTable();

		uint	ratio_;
		uint	aml_;
		uint	pml_;
		int		pmv_;
		OpType	optype_;
		uint32	multable_[4][16];
	};
}

#endif // FM_GEN_H
