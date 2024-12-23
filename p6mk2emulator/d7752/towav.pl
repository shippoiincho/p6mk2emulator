# 1�s1�T���v���ŋL�q���ꂽ�����f�[�^�� wav �t�@�C���ɕϊ�����

$SAMPLING_RATE = 44100;

open(IN, shift(@ARGV)) || die "�t�@�C��������܂���";
@samples = <IN>;
close(IN);

$nsamples = @samples;
print "$nsamples ����܂�";

open(OUT, ">".shift(@ARGV)) || die "�t�@�C�������܂���";
binmode(OUT);

# wave �w�b�_�̏�������

# RIFF �w�b�_
print OUT "RIFF";
print OUT pack('V', 4 + 8 * 2 + 18 + $nsamples * 2);

# WAVE �t�H�[�}�b�g
print OUT "WAVE";

# fmt �`�����N
print OUT "fmt ";
print OUT pack('V', 18);
print OUT pack('v', 1);   # wFormatTag
print OUT pack('v', 1);   # nChannels
print OUT pack('V', $SAMPLING_RATE);  # nSamplesPerSec;
print OUT pack('V', $SAMPLING_RATE * 2); # nAvgBytesPerSec
print OUT pack('v', 2);   # nBlockAlign;
print OUT pack('v', 16);  # wBitsPerSample;
print OUT pack('v', 0);   # cbSize

# data �`�����N
print OUT "data";
print OUT pack('V', $nsamples*2);
print OUT pack('v*', @samples);

close(OUT);

exit;
