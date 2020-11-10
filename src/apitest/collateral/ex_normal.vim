echo "hi"

function NormDeleteLine()
    exe "norm! dd"
endfunction

function NormInsertCharacter()
    exe "norm! Ia"
endfunction

function NormInsertCharacterBothSides()
    exe "norm! Ia\<ESC>Ab"
endfunction

function NormInsertCharacterBothSidesMultipleLines()
    exe "%norm! Ia\<ESC>Ab"
endfunction
