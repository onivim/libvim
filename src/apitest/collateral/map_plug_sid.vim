
function! s:sayhello(...)
  echo "Hello"
endfunction

nnoremap <silent> <Plug>TestCommand :<C-U>call <SID>sayhello()<CR>
