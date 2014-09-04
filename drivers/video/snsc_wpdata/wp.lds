SECTIONS
{
  .data.init :
  {
	__wp_start = .;
       *(.data)
	__wp_end = .;
  }
}

