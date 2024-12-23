# 1行1サンプルで記述された音声データを wav ファイルに変換する

$SAMPLING_RATE = 44100;

open(IN, shift(@ARGV)) || die "ファイルがありません";
@samples = <IN>;
close(IN);

$nsamples = @samples;
print "$nsamples あります";

open(OUT, ">".shift(@ARGV)) || die "ファイルを作れません";
binmode(OUT);

# wave ヘッダの書き込み

# RIFF ヘッダ
print OUT "RIFF";
print OUT pack('V', 4 + 8 * 2 + 18 + $nsamples * 2);

# WAVE フォーマット
print OUT "WAVE";

# fmt チャンク
print OUT "fmt ";
print OUT pack('V', 18);
print OUT pack('v', 1);   # wFormatTag
print OUT pack('v', 1);   # nChannels
print OUT pack('V', $SAMPLING_RATE);  # nSamplesPerSec;
print OUT pack('V', $SAMPLING_RATE * 2); # nAvgBytesPerSec
print OUT pack('v', 2);   # nBlockAlign;
print OUT pack('v', 16);  # wBitsPerSample;
print OUT pack('v', 0);   # cbSize

# data チャンク
print OUT "data";
print OUT pack('V', $nsamples*2);
print OUT pack('v*', @samples);

close(OUT);

exit;
