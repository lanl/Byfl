!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Do some simple, pointless work  !
! By Scott Pakin <pakin@lanl.gov> !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

program simple
  implicit none
  integer :: iters
  integer :: i
  integer :: sum
  character(len=32) :: arg
  character(len=16) :: fmt

  ! Read the number of iterations from the command line.
  if (command_argument_count() > 0) then
     call get_command_argument(1, arg)
     read(arg, '(i)') iters
  else
     iters = 100000
  end if

  ! Perform some hard-to-optimize-away work.
  do i = 0, iters - 1
     sum = sum*34564793 + i
  end do
  write(*, 100) sum
100 format ('Sum is ', I0)
end program
